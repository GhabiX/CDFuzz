
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dict_log_fpath;          /* Dictionary debug file path       */
static FILE *dict_fp = NULL;

void init_dict_log(char *out_dir) {
    dict_log_fpath = (char *) malloc(strlen(out_dir) + strlen("/dict_debug.log") + 16);
    sprintf(dict_log_fpath, "%s%s", out_dir, "/dict_debug.log");
    dict_fp = fopen(dict_log_fpath, "w+");
}

void dict_log(char *log) {
    fprintf(dict_fp, "%s\n", log);
}

void close_dict_log() {
    fclose(dict_fp);
}

// u8 get_extra_type(char *start_ptr){
//     if (strstr(start_ptr, "icmp_")) return 0; // ICMP_DICT
//     else if (strstr(start_ptr, "strcmp_")) return 1; // STRCMP_DICT
//     else return 2; // SWITCH_DICT
// }

u32 extract_sibling_edge(char *start_ptr) {
    char buf[256];
    memset(buf, 0, sizeof(buf));
    char *lptr = strstr(start_ptr, "siblingEdge_");
    char *rptr = strstr(start_ptr, "_val=");
    int id_len = rptr - lptr - 12;
    memcpy(buf, lptr + 12, id_len);
    return atoi(buf);
}

u32 extract_case_edge(char *start_ptr) {
    char buf[256];
    memset(buf, 0, sizeof(buf));
    char *lptr = strstr(start_ptr, "caseEdge_");
    char *rptr = strstr(start_ptr, "_val=");
    int id_len = rptr - lptr - 9;
    memcpy(buf, lptr + 9, id_len);
    printf("[DEBUG] extract_case_edge: %s\n", buf);
    return atoi(buf);
}

u32 extract_default_edge(char *start_ptr) {
    char buf[256];
    memset(buf, 0, sizeof(buf));
    char *lptr = strstr(start_ptr, "defaultEdge_");
    char *rptr = strstr(start_ptr, "_caseEdge");
    int id_len = rptr - lptr - 12;
    memcpy(buf, lptr + 12, id_len);
    printf("[DEBUG] extract_default_edge: %s\n", buf);
    return atoi(buf);
}