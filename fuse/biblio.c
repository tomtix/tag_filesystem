#include <string.h>
#include "tag.h"
#include "file.h"
#include "log.h"
#include "parse.h"

void update_lib(char *tagFile)
{
    struct list *filesList = file_list();
    FILE *lib = NULL;
    lib = fopen(tagFile, "w+");

    if (lib != NULL) {
        int s1 = list_size(filesList);
        for (int i = 1; i <= s1; i++) {
            struct file *fi = list_get(filesList, i);

            if (fi != NULL) {
                struct hash_table *fiTags = fi->tags;
                struct list *fiTagsList = ht_to_list(fiTags);

                fprintf(lib, "[%s]\n", fi->name);
                int s2 = list_size(fiTagsList);
                for (int j = 1; j <= s2; ++j) {
                    struct tag *t = list_get(fiTagsList, j);
                    if (t != NULL) {
                        fprintf(lib, "%s\n", t->value);
                    } else {
                        print_debug("la structure tag n'existe pas\n");
                    }
                }
            } else  {
                print_debug("la structure file n'existe pas\n");
            }
        }
        fclose(lib);
    }
}