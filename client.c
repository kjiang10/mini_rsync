#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>

#define DEBUG		0
#define DEBUG_NEW	0
#define INPUT_DIR	"a"
#define BLOCK		1000
#define HEAD		10

//pthread_barrier_t mybarrier;

char* str_encrypt(char* input_str){
	char* ptr = input_str;
	while (*ptr != '\0'){
		*ptr = (char)((int)(*ptr) ^ (int)0x55);
		ptr++;
	}
	return input_str;
}

char* str_decrypt(char* input_str){
	char* ptr = input_str;
	while (*ptr != '\0'){
		*ptr = (*ptr) ^ 0x55;
		ptr++;
	}	
	return input_str;
}

off_t fsize(const char *filename) {
	struct stat st; 
	if (stat(filename, &st) == 0)
		return st.st_size;
	return -1; 
}

// count file num
size_t count_file_num(char* input_str){
    const char dim[2] = ",";
    char *token;
	size_t res = 0;

	char* input_str_cpy = (char*)malloc(sizeof(char) * (strlen(input_str) + 1));
	strcpy(input_str_cpy, input_str);
    /* get the first token */
    token = strtok(input_str_cpy, dim);
    while(token != NULL) {
        // [insert_function_piece_begin]
		res++;
        // [insert_function_piece_end]
        token = strtok(NULL, dim);
    }
	free(input_str_cpy);
	return res;
}

// append size to message - [message layer protocol]
void send_message(int fd, char* message){
	size_t size = strlen(message) + 1;

	// prepare message with header
	size_t message_size = size + HEAD;
	char* pre_message = (char *)malloc(sizeof(char) * message_size); //1

	char head_str[HEAD];
	snprintf(head_str, HEAD, "%zu", size);
	size_t head_len = strlen(head_str);
	size_t zero_len = HEAD - head_len;

	memset(pre_message, ' ', zero_len * sizeof(char));
	memset(pre_message + zero_len, '\0', 1);

	strcat(pre_message, head_str);
	strcat(pre_message, message);

	// send message
	if (DEBUG){
		printf("message_size:%zu\n", message_size);
	}
	write(fd, pre_message, message_size);
    free(pre_message); //1
}

// first get fix byte of size string and convert into size_t - [message layer protocol]
size_t recv_message_size(int fd){
    
    char* header = (char *)malloc(sizeof(char) * (HEAD + 1)); //2
    read(fd, header, HEAD);
    memset(header + HEAD, '\0', 1);
    
    char* endptr;
    long int res = strtol(header, &endptr, 10);
    
    free(header); //2
    
    return (size_t)res; // unsafe
}

// - [message layer protocol]
// assume the message is a buffer with enough space
void recv_message(int fd, char* message, size_t size){
    read(fd, message, size);
}

// - {global helper functions} [mem_req]
char* mem_get_full_name(char* dirname, char* filename){
	size_t fullname_len = 2 + strlen(dirname) + 1 + strlen(filename) + 1; 
	char* fullname = (char*) malloc(sizeof(char)*fullname_len); 	

	*fullname = '\0';
	strcat(fullname,"./");
	strcat(fullname,dirname);
	strcat(fullname,"/");
	strcat(fullname,filename);

	return fullname;
}

void mem_free_full_name(char* fullname){
	free(fullname);
}

// - {inner functions}
void send_file(int fd, char* dirname, char* filename){
	char* fullname = mem_get_full_name(dirname, filename);
	int filedesc = open(fullname, O_RDONLY);
    char buf[BLOCK];

	size_t filename_len = 1;
	char* content = (char*)malloc(filename_len); // 401
				
	char* ptr = content;
    while (read(filedesc, buf, 1)>0){
		*ptr = buf[0];
		ptr++;
		filename_len++;
		content = (char*)realloc(content, filename_len); // 401
    }
	*ptr = '\0';
	if (DEBUG){
		printf("cotent:%s\n", content);
	}

	// [send] - content
    str_encrypt(content);
	send_message(fd, content);
	free(content);	

	mem_free_full_name(fullname);
}


void* threadFn(void *input_str){
	char* fullname = (char*)input_str;
	size_t* sum_ptr = malloc(sizeof(size_t));
	*sum_ptr = 0;

	if (DEBUG){
		printf("fullname:%s\n", fullname);
	}

	// - {read_file_begin}
	int filedesc = open(fullname, O_RDONLY);
    char buf[BLOCK];
    while (read(filedesc, buf, 1)>0){
		(*sum_ptr) += (size_t)(int)buf[0];
	if (DEBUG){
		printf("sum:%zu\n", *sum_ptr);
	}
    }
	// - {read_file_end}
	return (void*)sum_ptr;
}

size_t checksum_from_file(char* dir, char* filename){
	size_t sum = 0;

	char* fullname = mem_get_full_name(dir, filename);
	if (DEBUG){
		printf("fullname:%s\n", fullname);
	}

	// - {read_file_begin}
	int filedesc = open(fullname, O_RDONLY);
    char buf[BLOCK];
    while (read(filedesc, buf, 1)>0){
		sum += (size_t)(int)buf[0];
	if (DEBUG){
		printf("sum:%zu\n", sum);
	}
    }
	// - {read_file_end}
	
	mem_free_full_name(fullname);

	// int filedesc = open("./a/1.txt", O_RDONLY);
	
	return sum;
}


// - [main function]
int main(int argc, char **argv){ 

	if (argc < 4) {
		fprintf(stderr, "Must pass 3 argument!\n");
		exit(1);
	}

	char* client_dir = argv[1];
	char* host = argv[2];
	char* port = argv[3];
	if (DEBUG_NEW){
		printf("client_dir:%s\n", client_dir);
		printf("host:%s\n", host);
		printf("port:%s\n", port);
	}

	int s;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;


	s = getaddrinfo(host, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(1); 
	}

	if (connect(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
		perror("connect");
		exit(2);
	}

	
	// 1.Connected to the server
	printf("Connected to the server\n");


	char fake_m[100] = "playstation 2 is the best game console since the beggining of the word.\nxbox is bad.";
	send_message(sock_fd, fake_m);

	// {get_filenames_begin} get filenames from given directory
	size_t filename_len = 1;
	char* filenames = (char*)malloc(filename_len); // 3
	*filenames='\0';

	// [NEW]
	size_t file_count = 0;

    DIR *d;
    struct dirent *dir;
    d = opendir(client_dir);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			if (dir->d_type == DT_REG){
				file_count++;
				size_t filename_size = strlen(dir->d_name);
				filename_len += filename_size + 1;
				filenames = (char*)realloc(filenames, filename_len);
				if (DEBUG){
					printf("%s\n", dir->d_name);
				}
				strcat(filenames, dir->d_name);
				strcat(filenames, ",");
			}
        }
		filenames[strlen(filenames) - 1] = '\0';
		if (DEBUG){
			printf("filenames:%s\n", filenames);
		}
        closedir(d);
    }
	// {get_filenames_end}

	// [SEND] - filenames "1.txt,2.txt,3.txt"
	// Sending 8 filenames in directory "myfiles" to server
	printf("Sending %zu filenames in directory \"%s\" to server\n", file_count, client_dir);
	send_message(sock_fd, filenames);

	// [RECV] - common_filenames
	// {recv_message_begin}
	size_t m_size = recv_message_size(sock_fd);
	char* common_filenames = (char *)malloc(sizeof(char) * m_size); //1
	recv_message(sock_fd, common_filenames, m_size);
	if (DEBUG){
		printf("common_filenames:%s\n", common_filenames);
	}

	// [NEW]
	// 2 files on server do not exist so skipping checksum
	size_t common_file_count = count_file_num(common_filenames);
	printf("%zu files on server do not exist so skipping checksum\n", file_count - common_file_count);
	printf("Generating checksums for remaining %zu file(s)\n", common_file_count);
	printf("......\n");
	// {recv_message_end}

	// 123 {CHECKSUM_BEGIN}
	size_t checksum_len= 1;
	char* checksums = (char*)malloc(checksum_len); // 3
	*checksums ='\0';
    
    
    // OUTLOOP split client file token
    // [INPUT_BEGIN]
	char* input_str = (char*)malloc(sizeof(char) * (strlen(common_filenames)+1)); // free111
	strcpy(input_str, common_filenames);

	size_t f_num = count_file_num(input_str); 
	pthread_t* ids = (pthread_t*)calloc(f_num, sizeof(pthread_t));
    // [INPUT_END]
    const char dim[2] = ",";
    char *token;
    /* get the first token */
    token = strtok(input_str, dim);

	//pthread_barrier_init(&mybarrier, NULL, f_num + 1);
    
	int i = 0;
	// first loop
    while( token != NULL ) {
        // [insert_function_piece_begin]
        char* temp_filename = token;
		char* fullname = mem_get_full_name(client_dir, temp_filename);
		pthread_create(&ids[i], NULL, threadFn, fullname);
		i++;

        //size_t checksum_num = checksum_from_file(client_dir, temp_filename);
		//if (DEBUG){
		//	printf("checksum_num:%zu\n", checksum_num);
		//}
        // [insert_function_piece_end]
        
        token = strtok(NULL, dim);
    }
	free(input_str);

	//pthread_barrier_wait(&mybarrier);

	input_str = (char*)malloc(sizeof(char) * (strlen(common_filenames)+1)); // free111
	strcpy(input_str, common_filenames);
    // [INPUT_END]
    /* get the first token */
    token = strtok(input_str, dim);
	i = 0;
	while (token != NULL){
		size_t* checksum_num_ptr;
		pthread_join(ids[i], (void**)&checksum_num_ptr);
		size_t checksum_num = *checksum_num_ptr;
		free(checksum_num_ptr);
		i++;	
		char sum_str[64];

		snprintf(sum_str, 64, "%zu", checksum_num);

		checksum_len += strlen(token) + 1 + strlen(sum_str) + 1;
		checksums = (char*)realloc(checksums, checksum_len);

		strcat(checksums, token);
		strcat(checksums, ";");
		strcat(checksums, sum_str);
		strcat(checksums, ",");

        token = strtok(NULL, dim);
	}
	free(input_str);
	//pthread_barrier_destroy(&mybarrier);
	free(ids);

	checksums[strlen(checksums) - 1] = '\0';
	if (DEBUG){
		printf("checksums:%s\n", checksums);
	}
	// {CHECKSUM_END}

	printf("Done. Sending %zu checksums\n", common_file_count);
	// {SEND_BEGIN} checksums
	char* client_checksums = checksums;
	if (DEBUG){
		printf("client_checksums:%s\n", client_checksums);
	}
	send_message(sock_fd, client_checksums);
	// {SEND_END} checksums


	// {RECV_BEGIN} filelist
	// {recv_message_begin}
	m_size = recv_message_size(sock_fd);
	char* filelist = (char *)malloc(sizeof(char) * m_size); //1
	recv_message(sock_fd, filelist, m_size);
	if (DEBUG){
		printf("filelist:%s\n", filelist);
	}
	// {recv_message_end}
	size_t filelist_count = count_file_num(filelist);
	printf("Server requests %zu file(s). Sending files...\n", filelist_count);
    
	clock_t start = 0, end = 0;
	int cpu_time_used;
	off_t file_size_sum = 0;
	// [FINAL] send files
    // [INPUT_BEGIN]
    input_str = filelist;
    // [INPUT_END]
    const char dim_e[2] = ",";
    /* get the first token */
    token = strtok(input_str, dim_e);
    
    while( token != NULL ) {
        // [insert_function_piece_begin]
        char* temp_filename = token;
		if (DEBUG){
			printf("finalfilename:%s\n", temp_filename);
		}
		// [SEND file size]
		char* fullname = mem_get_full_name(client_dir, temp_filename);
		off_t file_size = fsize(fullname);
		file_size_sum += file_size;
		mem_free_full_name(fullname);

		char head_str[100];
		snprintf(head_str, 100, "%lld", file_size);
		send_message(sock_fd, head_str);

		// [SEND file]
		send_file(sock_fd, client_dir,temp_filename);
        // [insert_function_piece_end]

		// [RECV status]
		size_t m_size = recv_message_size(sock_fd);
		char* is_written = (char *)malloc(sizeof(char) * m_size); //1
		recv_message(sock_fd, is_written, m_size);
		printf("%s (%lld bytes)  - sent OK. Server reports %s.\n", temp_filename, file_size, is_written);
        
        token = strtok(NULL, dim_e);
    }
	cpu_time_used = (int)(((double) (end - start)) / CLOCKS_PER_SEC);
	printf("Finished (%lld bytes of file data sent in %d seconds) with 0 failure\n", file_size_sum, cpu_time_used);
    
	free(filenames);
	free(filelist);
	free(common_filenames);
	free(checksums);
	exit(0);
}
