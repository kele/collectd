#include "plugin.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <wbemidl.h>
#include <windows.h>

#include "wmi.h"
#include "wmi_variant_utils.h"

wchar_t* strtowstr(const char *source)
{
    int source_len;
    wchar_t *result;

    source_len = strlen (source);
    result = calloc (source_len + 1, sizeof (wchar_t));
    mbstowcs (result, source, source_len + 1);

    return (result);
}

char* wstrtostr (const wchar_t *source)
{
    int source_len;
    char *result;
    
    source_len = wcslen (source);
    result = calloc (source_len + 1, sizeof (char));
    wcstombs (result, source, source_len + 1);
    return (result);
}


static LIST_TYPE(plugin_instance_t) *plugin_instances;


typedef struct wmi_connection_s
{
    IWbemServices *services;
    IWbemLocator *locator;
    BSTR resource;
    BSTR language;

} wmi_connection_t;

typedef struct wmi_results_s
{
    IEnumWbemClassObject *results;
    ULONG _returned_count;
} wmi_results_t;

typedef struct wmi_result_s
{
    IWbemClassObject* result;
} wmi_result_t;

wmi_results_t* wmi_query (wmi_connection_t *connection, const wchar_t *query)
{
    HRESULT hr;

    wmi_results_t *res = malloc (sizeof (wmi_results_t));
    BSTR query_bstr = SysAllocString (query);
    hr = connection->services->lpVtbl->ExecQuery(
            connection->services,
            connection->language,
            query_bstr,
            WBEM_FLAG_FORWARD_ONLY,
            NULL,
            &res->results);
    SysFreeString (query_bstr);
    res->_returned_count = 0;

    if (hr == S_OK)
        return (res);

    switch (hr)
    {
    case WBEM_E_ACCESS_DENIED:
        ERROR ("wmi error: Access denied while querying. Query: %ls", query);
        break;
    case WBEM_E_INVALID_QUERY: 
        ERROR ("wmi error: Invalid query: '%ls'", query);
        break;
    default:
        ERROR ("wmi error: Unknown error during query: %ls", query);
    }
    return (NULL);
}

void wmi_results_release (wmi_results_t *results)
{
    if (results)
        results->results->lpVtbl->Release (results->results);
}

wmi_result_t* wmi_get_next_result (wmi_results_t *results)
{
    if (!results)
        return (NULL);

    wmi_result_t *result = malloc (sizeof (wmi_result_t));
    HRESULT hr = results->results->lpVtbl->Next(
            results->results,
            WBEM_INFINITE, // TODO: maybe this shouldn't block infinitely?
            1,
            &result->result,
            &results->_returned_count);

    if (hr == S_OK)
        return (result);
    else
    {
        free (result);
        return (NULL);
    }
}

void wmi_result_release(wmi_result_t *result)
{
    result->result->lpVtbl->Release (result->result);
}

int wmi_result_get_value(const wmi_result_t *result, const wchar_t *name, VARIANT *value)
{
    HRESULT hr = result->result->lpVtbl->Get(result->result, name, 0, value, 0, 0);

    if (hr == S_OK)
        return 0;
    
    switch (hr)
    {
    case WBEM_E_NOT_FOUND:
        ERROR("wmi error: Property %ls not found.", name);
        break;
    default:
        ERROR("wmi error: Unknown error while fetching property %ls", name);
        break;
    }

    return -1;
}

void wmi_release(wmi_connection_t *connection)
{
    if (!connection)
        return;
    
    if (connection->services)
        connection->services->lpVtbl->Release(connection->services);

    if (connection->locator)
        connection->locator->lpVtbl->Release(connection->locator);

    CoUninitialize();

    SysFreeString(connection->language);
    SysFreeString(connection->resource);
    free (connection);
}

wmi_connection_t* wmi_connect()
{
    wmi_connection_t *connection = malloc(sizeof (wmi_connection_t));
    memset(connection, 0, sizeof (*connection));

    connection->services = NULL;
    connection->locator = NULL;
    connection->resource = SysAllocString(L"ROOT\\CIMV2");
    connection->language = SysAllocString(L"WQL");

    HRESULT hr;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (hr != S_OK)
        goto err;

    hr = CoInitializeSecurity(
            NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL);
    if (hr != S_OK)
        goto err;

    hr = CoCreateInstance(
           &CLSID_WbemLocator,
           0,
           CLSCTX_INPROC_SERVER,
           &IID_IWbemLocator,
           (LPVOID *) &connection->locator);
    if (hr != S_OK)
        goto err;

    hr = connection->locator->lpVtbl->ConnectServer(
            connection->locator,
            connection->resource,
            NULL,
            NULL,
            NULL,
            0,
            NULL,
            NULL,
            &connection->services);
    if (hr != S_OK)
        goto err;

    return connection;

err:
    ERROR("wmi error: Initialization failed. Error code: %d", (int)hr);
    wmi_release(connection);
    return NULL;
}

static wmi_connection_t *wmi;

static int wmi_init (void)
{
    wmi = wmi_connect();
    return (0);
}

static int wmi_shutdown (void)
{
    // TODO; make a proper cleanup
    LIST_FREE(plugin_instances);
    wmi_release(wmi);
    return (0);
}

static void store (VARIANT *src, value_t *dst, int dst_type)
{
    switch (dst_type)
    {
    case DS_TYPE_GAUGE:
        dst->gauge = variant_get_double (src);
        break;

    case DS_TYPE_DERIVE:
        dst->derive = variant_get_int64 (src);
        break;

    case DS_TYPE_ABSOLUTE:
        dst->absolute = variant_get_uint64 (src);
        break;

    case DS_TYPE_COUNTER:
        dst->counter = variant_get_ull (src);
        break;

    default:
        // TODO: error handling here? nah...
        break;
    }
}

/* Find position of `name` in `ds */
static int find_index_in_ds (const data_set_t *ds, const char *name)
{
    int i;
    for (i = 0; i < ds->ds_num; i++)
        if (strcmp (ds->ds[i].name, name) == 0)
            return (i);
    return (-1);
}

static int wmi_read (void)
{
    // TODO: this looks so ugly :(
    LIST_TYPE(plugin_instance_t) *pn;
    for (pn = plugin_instances; pn != NULL; pn = LIST_NEXT(pn))
    {
        plugin_instance_t *pi = LIST_NODE(pn);

        LIST_TYPE(wmi_query_t) *qn;
        for (qn = pi->queries; qn != NULL; qn = LIST_NEXT(qn))
        {
            wmi_query_t *q;
            wmi_results_t *results;
            value_list_t vl = VALUE_LIST_INIT;

            q = LIST_NODE(qn);

            sstrncpy (vl.host, hostname_g, sizeof (vl.host));
            sstrncpy (vl.plugin, "wmi", sizeof (vl.plugin));
            sstrncpy (vl.plugin_instance, q->plugin_instance->base, sizeof (vl.plugin_instance));

            results = wmi_query(wmi, q->statement);        

            wmi_result_t *result;
            while ((result = wmi_get_next_result (results)))
            {
                LIST_TYPE(wmi_metric_t) *mn;
                for (mn = q->metrics; mn != NULL; mn = LIST_NEXT(mn))
                {
                    value_t *values;
                    const data_set_t *ds;
                    int i;
                    VARIANT v;
                    wmi_metric_t *m = LIST_NODE(mn);

                    values = calloc (m->values_num, sizeof (value_t));
                    ds = plugin_get_ds (m->typename);

                    for (i = 0; i < m->values_num; i++)
                    {
                        int index_in_ds;
                        wmi_result_get_value (result, m->values[i].source, &v);

                        index_in_ds = find_index_in_ds (ds, m->values[i].dest);
                        if (index_in_ds != -1)
                            store (&v, &values[i], ds->ds[index_in_ds].type);
                        else
                            ERROR ("wmi error: Cannot find field %s in type %s",
                                    m->values[i].dest, ds->type);
                    }

                    vl.values_len = m->values_num;
                    vl.values = values;
                    sstrncpy (vl.type, m->typename, sizeof (vl.type));
                    sstrncpy (vl.type_instance, m->type_instance, sizeof (vl.type_instance));
                    plugin_dispatch_values (&vl);
                    free (values);
                }
                wmi_result_release(result);
            }
            wmi_results_release(results);
        }
    }
    return (0);
}

static int wmi_configure_wrapper (oconfig_item_t *ci)
{
    // TODO: change it to multiple read callback registrations,
    // one per instance
    return (wmi_configure (ci, &plugin_instances));
}

void module_register (void)
{
    plugin_register_complex_config ("wmi", wmi_configure_wrapper);
    plugin_register_init ("wmi", wmi_init);
    plugin_register_read ("wmi", wmi_read);
    plugin_register_shutdown ("wmi", wmi_shutdown);
}
