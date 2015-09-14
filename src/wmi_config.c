#include "configfile.h"
#include "wmi.h"
#include "plugin.h"

static char* config_get_typename (oconfig_item_t *ci)
{
    int i;
    char *typename = NULL;
    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp ("Type", child->key) == 0)
        {
            if (typename)
            {
                ERROR ("wmi error: Multiple Types provided in one block.");
                free (typename);
                return (NULL);
            }

            if (child->values_num != 1 
                    || child->values[0].type != OCONFIG_TYPE_STRING)
            {
                ERROR ("wmi error: Type needs a single string argument,");
                return (NULL);
            }

            typename = strdup (child->values[0].value.string);
        }
    }

    if (!typename)
    {
        ERROR ("wmi error: Type declaration not found in block.");
        return (NULL);
    }

    return (typename);
}

const char *metric_supported_options[] =
{
    "TypeInstance",
    "TypeInstanceSuffixFrom",
    "Value",
    "Type"
};


static int config_get_metric_sanity_check (oconfig_item_t *ci)
{
    int i;

    assert (strcmp ("Metric", ci->key) == 0);

    for (i = 0; i < ci->children_num; i++)
    {
        int j;
        int found = 0;
        oconfig_item_t *child = &ci->children[i];

        for (j = 0; j < COUNTOF (metric_supported_options); j++)
        {
            if (strcmp (metric_supported_options[j], child->key) == 0)
            {
                found = 1;
                break;
            }
        }

        if (!found)
        {
            ERROR ("%s option is not supported in Metric block!", child->key);
            return (-1);
        }
    }

    return (0);
}

static metadata_str_t* config_get_metadata_str (oconfig_item_t *ci,
        const char *base_str, const char *part_str)
{
    int i;
    int num_parts, read_froms;
    metadata_str_t* ms = NULL;

    num_parts = 0;
    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp (part_str, child->key) == 0)
            num_parts++;
    }

    ms = metadata_str_alloc (num_parts);

    read_froms = 0;
    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp (base_str, child->key) == 0)
        {
            if (ms->base)
            {
                ERROR ("wmi error: multiple %ss provided "
                        "in one Metric block,", base_str);
                metadata_str_free (ms);
                return (NULL);
            }

            if (cf_util_get_string (child, &ms->base))
            {
                ERROR ("wmi error: %s needs a single string argument.",
                        base_str);
                return (NULL);
            }
        }
        else if (strcmp (part_str, child->key) == 0)
        {
            char *str = NULL;
            if (cf_util_get_string (child, &str))
            {
                ERROR ("wmi error: %s needs a single string argument.",
                        part_str);
                metadata_str_free (ms);
                return (NULL);
            }

            ms->parts[read_froms] = strtowstr (str);
            free (str);
            read_froms++;
        }
    }

    return (ms);
}

static metadata_str_t* config_get_type_instance_str (oconfig_item_t *ci)
{
    return (config_get_metadata_str (ci, "TypeInstance", "TypeInstanceSuffixFrom"));
}

static int config_get_number_of_values (oconfig_item_t *ci)
{
    int i;
    int values_num = 0;
    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp ("Value", child->key) == 0)
        {
            if (child->values_num != 2 
                    || child->values[0].type != OCONFIG_TYPE_STRING
                    || child->values[1].type != OCONFIG_TYPE_STRING)
            {
                ERROR ("wmi error: Value expects exactly two string arguments: "
                        "name of the field in the object and the name "
                        "in collectd type.");
                return (-1);
            }
            values_num++;
        }
    }

    if (values_num == 0)
    {
        ERROR ("wmi error: At least one Value in Metric block is needed.");
        return (-1);
    }
    return (values_num);
}

static wmi_metric_t* config_get_metric (oconfig_item_t *ci)
{
    int i;
    char *typename;
    metadata_str_t *type_instance;
    wmi_metric_t *metric = NULL;
    int values_num = 0;

    if (config_get_metric_sanity_check(ci))
        return (NULL);

    if ((typename = config_get_typename (ci)) == NULL)
        goto err;

    if ((type_instance = config_get_type_instance_str (ci)) == NULL)
        goto err;

    if ((values_num = config_get_number_of_values (ci)) <= 0)
        goto err;


    metric = wmi_metric_alloc (values_num);
    metric->values_num = 0;
    metric->typename = typename;
    metric->type_instance = type_instance;

    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp ("Value", child->key) == 0)
        {

            metric->values[metric->values_num].source =
                strtowstr (child->values[0].value.string);
            metric->values[metric->values_num].dest =
                strdup (child->values[1].value.string);
            metric->values_num++;
        }
    }

    return (metric);

err:
    free (typename);
    free (type_instance);
    return (NULL);
}

static wmi_query_t* config_get_query(oconfig_item_t *ci, plugin_instance_t *pi)
{
    int i;
    int status = 0;
    char *stmt = NULL;
    int stmt_len;
    wmi_query_t *query = NULL;
    LIST_TYPE(wmi_metric_t) *metrics = NULL;

    assert (strcmp ("Query", ci->key) == 0);

    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp ("Statement", child->key) == 0)
        {
            if (stmt)
            {
                ERROR ("wmi error: Multiple Statements in one Query block."
                        " Previous: %s", stmt);
                status = -1;
                break;
            }
            if (cf_util_get_string (child, &stmt))
            {
                ERROR("wmi error: Statement requires a single "
                        "string as an argument.");
                status = -1;
                break;
            }
            stmt_len = strlen (stmt);
        }
        else if (strcmp ("Metric", child->key) == 0)
        {
            wmi_metric_t *m = config_get_metric (child);
            if (m) LIST_INSERT_FRONT (metrics, m);
        }
        else
        {
            status = -1;
            ERROR ("wmi error: unknown option: %s", child->key);
            break;
        }
    }

    if (stmt == NULL || metrics == NULL)
        status = -1;

    if (status)
    {
        LIST_FREE (metrics, wmi_metric_free);
        return NULL;
    }

    query = malloc (sizeof (wmi_query_t));
    query->statement = calloc (stmt_len, sizeof (wchar_t));
    mbstowcs (query->statement, stmt, stmt_len);
    free (stmt);
    query->metrics = metrics;
    query->plugin_instance = pi;
    return (query);
}

static int add_instance (oconfig_item_t *ci,
        LIST_TYPE(plugin_instance_t) **plugin_instances)
{
    int i;
    // TODO; error handling
    assert (strcmp ("Instance", ci->key) == 0);

    plugin_instance_t *pi = malloc (sizeof (plugin_instance_t));
    if (cf_util_get_string (ci, &pi->base_name))
    {
        free (pi);
        return (-1);
    }

    LIST_INSERT_FRONT (*plugin_instances, pi);

    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];
        if (strcmp ("Query", child->key) == 0)
        {
            wmi_query_t *q = config_get_query(child, pi);
            if (!q)
                continue;
            
            LIST_INSERT_FRONT(pi->queries, q);
        }
    }

    return (0);
}

int wmi_configure (oconfig_item_t *ci,
        LIST_TYPE(plugin_instance_t) **plugin_instances)
{
    int i;
    int status;
    int success = 0;

    for (i = 0; i < ci->children_num; i++)
    {
        oconfig_item_t *child = &ci->children[i];

        if (strcmp ("Instance", child->key) == 0)
        {
            status = add_instance(child, plugin_instances);
            if (!status)
                success = 1;
        }
    }

    if (success)
        return (0);
    else
    {
        ERROR("wmi error: No Instance has been added.");
        return (-1);
    }
}

void wmi_query_free(wmi_query_t *q)
{
    if (q)
    {
        free (q->statement);
        free (q->plugin_instance);
        LIST_FREE(q->metrics, wmi_metric_free);
    }
    free (q);
}

wmi_metric_t *wmi_metric_alloc(int num_values)
{
    int size = sizeof (wmi_metric_t) + num_values * sizeof (wmi_value_t);
    wmi_metric_t *m = malloc (size);
    memset (m, 0, size);
    m->values_num = num_values;
    return (m);
}

void wmi_metric_free(wmi_metric_t *m)
{
    if (m)
    {
        free (m->typename);
        free (m->type_instance);
    }
    free (m);
}

__attribute__ ((unused))
void wmi_value_free(wmi_value_t *w)
{
    if (w)
    {
        free (w->source);
        free (w->dest);
    }
    free (w);
}
