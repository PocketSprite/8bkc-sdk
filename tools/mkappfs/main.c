#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "appfs.h"
#include <unistd.h>
#include <stdlib.h>

#define OUTF "part-40-2.img"

void createFile(size_t len) {
	unlink(OUTF);
	FILE *f=fopen(OUTF, "w");
	printf("Generating part of %d bytes...\n", len);
	char buf[128];
	memset(buf, 0xff, 128);
	while(len) {
		size_t sz=len;
		if (sz>128) sz=128;
		len-=sz;
		fwrite(buf, sz, 1, f);
	}
	fclose(f);
}

void trimToFile(char *in, char *out) {
	unsigned char buff[1024];
	FILE *fin=fopen(in, "rb");
	FILE *fout=fopen(out, "w");
	int last=0, p=0;
	while(!feof(fin)) {
		fread(buff, 1024, 1, fin);
		for (int i=0; i<1024; i++) {
			if (buff[i] != 0xff) {
				last=p+i;
			}
		}
		p+=1024;
	}
	printf("Trimming output file to %dK...\n", last/1024);
	fseek(fin, 0, SEEK_SET);
	for (int i=0; i<=last; i+=1024) {
		fread(buff, 1024, 1, fin);
		fwrite(buff, 1024, 1, fout);
	}
	fclose(fin);
	fclose(fout);
}


void main(int argc, void **argv) {
	esp_err_t r;
	if (argc<3) {
		printf("Usage: %s size-in-bytes file1 [file2 ...]\n", argv[0]);
	}

	createFile(strtol(argv[1], NULL, 0));
	r=appfsInit(40, 2);

	for (int i=2; i<argc; i++) {
		FILE *f=fopen(argv[i], "rb");
		if (f==NULL) {
			perror(argv[i]);
			exit(1);
		}
		fseek(f, 0, SEEK_END);
		int sz=ftell(f);
		fseek(f, 0, SEEK_SET);
		printf("Adding %s size %d...\n", argv[i], sz);
		appfs_handle_t h;
		r=appfsCreateFile(argv[i], sz, &h);
		r=appfsErase(h, 0, (sz+4095)&(~4095));
		int off=0;
		while (off<sz) {
			unsigned char buf[1024];
			int len=sz-off;
			if (len>sizeof(buf)) len=sizeof(buf);
			size_t rsz=fread(buf, len, 1, f);
			assert(rsz==1);
			printf("Write to appfs file: offset %x len %x\n", off, len);
			r=appfsWrite(h, off, buf, len);
			assert(r==ESP_OK);
			off+=len;
		}
	}

	trimToFile(OUTF, "appfs.img");
}
