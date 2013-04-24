#include<limits.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>

#define MAX_FILES 31

typedef struct {
	char name[16];
	unsigned int start;
	unsigned int size;
} entry_t;

struct index {
	unsigned int size;
	unsigned int entry_number;
	entry_t entries[31];
};

struct file_data {
	unsigned int size;
	char name[16];
	char* data;
};

int pad(FILE* file, int num) {
	int i;
	for(i = 0; i < num; i++)
		putc(0x0, file);
	return 1;
}

int magic(FILE* file) {
	putc(0xFF, file);
	putc(0xFF, file);
	putc(0xFF, file);
	putc(0xFF, file);
	return 1;
}

int write_entry(FILE* file, entry_t* entry) {
	magic(file);
	fwrite((void*) entry->name, 1, 16, file);
	fwrite((void*) &entry->start, 4, 1, file);
	fwrite((void*) &entry->size, 4, 1, file);
	pad(file, 4);
	return 1;
}

int read_entry(FILE* file, entry_t* entry) {
	fseek(file, 4, SEEK_CUR);
	fread((void*) entry->name, 1, 16, file);
	fread((void*) &entry->start, 4, 1, file);
	fread((void*) &entry->size, 4, 1, file);
	fseek(file, 4, SEEK_CUR);
	return 1;
}

int write_index(FILE* file, struct index* file_index) {
	rewind(file);
	magic(file);
	fwrite((void*) &file_index->size, 4, 1, file);
	fwrite((void*) &file_index->entry_number, 4, 1, file);
	pad(file, 20);
	if(file_index->entry_number > MAX_FILES) {
		printf("ERROR: Too many files\n");
		printf("There are %d entries in this file\n", file_index->entry_number);
		exit(EXIT_FAILURE);
	}
	int i;
	for(i = 0; i < file_index->entry_number; i++) {
		write_entry(file, &file_index->entries[i]);
	}
	for(; i < MAX_FILES; i++) {
		pad(file, 32);
	}
	return 1;
}

int read_index(FILE* file, struct index* file_index) {
	rewind(file);
	fseek(file, 4, SEEK_CUR);
	fread((void*) &file_index->size, 4, 1, file);
	fread((void*) &file_index->entry_number, 4, 1, file);
	fseek(file, 20, SEEK_CUR);
	if(file_index->entry_number > MAX_FILES) {
		printf("ERROR: Too many files\n");
		printf("There are %d entries in this file\n", file_index->entry_number);
		exit(EXIT_FAILURE);
	}
	int i;
	for(i = 0; i < file_index->entry_number; i++) {
		read_entry(file, &file_index->entries[i]);
	}
	return 1;
}

int write_file(FILE* file, struct file_data* data, unsigned int offset) {
	fseek(file, offset, SEEK_SET);
	magic(file);
	if(fwrite((void*) &data->size, 4, 1, file) != 1) {	//Write size
		printf("Failed to write size\n");
		exit(EXIT_FAILURE);
	}
	if(fwrite((void*) data->name, 1, 16, file) != 16) { //Write name
		printf("Failed to write name\n");
		exit(EXIT_FAILURE);
	}
	pad(file, 8);
	if(fwrite((void*) data->data, 1, data->size, file) != data->size) { //Write actual data
		printf("Failed to write data\n");
		exit(EXIT_FAILURE);
	}
}

int initialize(FILE* file) {
	rewind(file);
	struct index file_index = {
		.size = 128 * 1024 * 1024,
		.entry_number = 0,
	};
	write_index(file, &file_index);
	rewind(file);
	return 1;
}

int is_initialized(FILE* file) {
	rewind(file);
	unsigned int magic_number;
	fread((void*) &magic_number, 4, 1, file);
	rewind(file);
	printf("The magic number is: %x\n", magic_number);
	return magic_number == 0xFFFFFFFF;
}

int main(int argc, char* argv[]) {
	char* file_name = argv[1];
	char* dev_name = argv[2];
	FILE* dev = fopen(dev_name, "r+b");
	if(dev == NULL) {
		dev = fopen(dev_name, "w+b");
	}
	FILE* read_file = fopen(file_name, "rb");
	if(!is_initialized(dev)) {
		initialize(dev);
		printf("The file has been initialized\n");
	}
	fseek(read_file, 0, SEEK_END);
	size_t size = ftell(read_file);
	struct file_data data = {
		.size = (unsigned int) size,
		.data = malloc(size),
	};
	strcpy(data.name, file_name);
	struct index file_index;
	rewind(read_file);
	fread((void*) data.data, 1, size, read_file);
	read_index(dev, &file_index);
	entry_t last_entry = {
		.name = "",
		.start = 1024,
		.size = 0,
	};
	int i;
	for(i = 0; i < file_index.entry_number; i++) {
		if(file_index.entries[i].start > last_entry.start) {
			last_entry = file_index.entries[i];
		}
	}
	int offset = last_entry.start + last_entry.size + 32;
	int to_add = offset % 512;
	offset += 512 - to_add;
	entry_t new_entry = {
		.start = offset,
		.size = size,
	};
	strcpy(new_entry.name, file_name);
	file_index.entries[file_index.entry_number] = new_entry;
	file_index.entry_number++;
	write_file(dev, &data, offset);
	write_index(dev, &file_index);
	return 0;
}

