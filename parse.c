#include <string.h>
#include "tag.h"
#include "file.h"
#include "log.h"
#include "parse.h"

/*#include "cutil/hash_table.h"
#include "cutil/list.h"
#include "cutil/string2.h"*/

#define MAX_LENGTH 1000


void parsing(const char* fileName) {
	
	char word[MAX_LENGTH] = "";

	FILE *fi = NULL;
	fi = fopen(fileName, "r+");
	struct file * f = NULL;
	if (fi != NULL) {
		while(fgets(word, MAX_LENGTH, fi) != NULL) {
			int i=0;
			while(word[i] == ' ')
				i++;	
			if (word[i] == '[') {
				char * data = copy(word+i+1, ']');
				f = file_get_or_create(data);
			} else {
				char * datat = copy(word+i, '\0');
				struct tag * tg = tag_get_or_create(datat);
				file_add_tag(f, tg);
				tag_add_file(tg,f);
			}
			
		}
	}

	fclose(fi);

}

char * copy(char word[], char end) {
	int length = 0;

	for (length; word[length] != end; length++);

	char * data = malloc(sizeof(char)*length);
	char* final = data;

	int j = 0;
	for ( j ; j<length ; j++ ) {
		*data = word[j];
		data++;
	}

	return final;
}
