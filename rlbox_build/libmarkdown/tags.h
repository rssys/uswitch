/* block-level tags for passing html blocks through the blender
 */
#ifndef _TAGS_D
#define _TAGS_D

#ifdef __cplusplus
extern "C" {
#endif

struct kw {
    char *id;
    int  size;
    int  selfclose;
} ;


struct kw* mkd_search_tags(char *, int);
void mkd_prepare_tags();
void mkd_deallocate_tags();
void mkd_sort_tags();
void mkd_define_tag(char *, int);

#ifdef __cplusplus
}
#endif


#endif
