#ifndef WMI_H
#define WMI_H

#define LIST_NODE_DECL(type) type _node
#define LIST_NEXT_DECL(type) type *_next
#define LIST_NODE(node) (node->_node)
#define LIST_NEXT(node) (node->_next)
#define LIST_HEAD(list) (list)

#define LIST_DECL_TYPE(node_type) \
struct list_ ## node_type ## _s; \
typedef struct list_ ## node_type ## _s list_ ## node_type ## _t;

#define LIST_DEF_TYPE(node_type) \
typedef struct list_ ## node_type ## _s \
{\
    LIST_NODE_DECL(node_type*); \
    LIST_NEXT_DECL(struct list_ ## node_type ## _s); \
} list_ ## node_type ## _t

#define LIST_TYPE(type) list_ ## type ## _t

#define LIST_INSERT_FRONT(list, new_node) \
do { \
    __typeof__(list) _n = malloc (sizeof (__typeof__(*list))); \
    LIST_NEXT(_n) = list; \
    LIST_NODE(_n) = new_node; \
    list = _n; \
} while (0)

#define LIST_FREE(list, node_free) \
do { \
    __typeof__(list) _head = list; \
    while (_head != NULL) \
    { \
        __typeof__(_head) _next = LIST_NEXT(_head); \
        node_free (LIST_NODE(_head)); \
        free (_head); \
        _head = _next; \
    } \
} while (0)

#define COUNTOF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))


typedef struct metadata_str_s
{
    char *base;
    int num_parts;
    wchar_t *parts[0];
} metadata_str_t;
metadata_str_t* metadata_str_alloc(int num_parts);
void metadata_str_free (metadata_str_t *ms);

struct wmi_query_s;
typedef struct wmi_query_s wmi_query_t;
LIST_DECL_TYPE(wmi_query_t);
typedef struct plugin_instance_s
{
    char *base_name;
    LIST_TYPE(wmi_query_t) *queries;
} plugin_instance_t;
LIST_DEF_TYPE(plugin_instance_t);
void plugin_instance_free (plugin_instance_t *pi);

typedef struct wmi_value_s
{
    wchar_t *source;
    char *dest;
} wmi_value_t;
void wmi_value_free(wmi_value_t *w);

typedef struct wmi_metric_s
{
    char *typename;
    metadata_str_t *type_instance;
    int values_num;
    wmi_value_t values[0];
} wmi_metric_t;
LIST_DEF_TYPE(wmi_metric_t);
wmi_metric_t *wmi_metric_alloc(int num_values);
void wmi_metric_free(wmi_metric_t *m);

typedef struct wmi_query_s
{
    wchar_t *statement;
    LIST_TYPE(wmi_metric_t) *metrics;

    plugin_instance_t *plugin_instance;
} wmi_query_t;
LIST_DEF_TYPE(wmi_query_t);
void wmi_query_free (wmi_query_t *q);

wchar_t* strtowstr (const char *source);
char* wstrtostr (const wchar_t *source);

int wmi_configure (oconfig_item_t *ci,
        LIST_TYPE(plugin_instance_t) **plugin_instances);

#endif /* WMI_H */
