#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>
#include <pthread.h>

#define DEBUG		0
#define DEBUG_NEW 	0
#define INPUT_DIR   "b"
#define BLOCK    	1000
#define HEAD    	10

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

char* str_encrypt(char* input_str){
    char* ptr = input_str;
    while (*ptr != '\0'){
        *ptr = (*ptr) ^ 0x55;
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
        //        printf("token:%s\n", token);
        // [insert_function_piece_begin]
        res++;
        // [insert_function_piece_end]
        token = strtok(NULL, dim);
    }
    free(input_str_cpy);
    return res;
}
// [GLOBAL_FUNCTION_BEGIN]
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
// [GLOBAL_FUNCTION_END]

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


// [LOCAL_FUNCTION_BEGIN]
bool is_file_on_server(char* filename, char* server_filenames){
	bool res = false;
	size_t str_size = strlen(server_filenames) + 1;
	char* temp_filenames = (char*)malloc(sizeof(char) * str_size); //909
	strcpy(temp_filenames, server_filenames);

    // OUTLOOP split client file token
    // [INPUT_BEGIN]
	if (DEBUG){
		printf("%s\n", server_filenames);
		printf("%s\n", temp_filenames);
	}
    char* input_str = temp_filenames;
    // [INPUT_END]
    const char dim[2] = ",";
	if (DEBUG){
	
	}
    char *token;
    
    /* get the first token */
    token = strtok(input_str, dim);
    
	if (DEBUG){
    	printf("is_file_client%s\n", filename);
	}
    /* walk through other tokens */
    while( token != NULL ) {
		// [insert_function_piece_begin]
		int ret = strcmp(filename, token);
		if (DEBUG){
        	printf( "is_file_server:%s\n", token );
		}
		if (ret == 0){
			res = true;	
			break;
		}
		// [insert_function_piece_end]
        
        token = strtok(NULL, dim);
    }
	
	free(temp_filenames);//909
	return res;
}

// [LOCAL_FUNCTION_END]

// - [message layer protocol]
// assume the message is a buffer with enough space
void recv_message(int fd, char* message, size_t size){
    read(fd, message, size);
}

int main(int argc, char **argv){

	if (argc < 3) {
		fprintf(stderr, "Must pass 2 argument!\n");
		exit(1);
	}

	char* client_dir = argv[1];
	char* port = argv[2];

	if (DEBUG_NEW){
		printf("client_dir:%s\n", client_dir);
		printf("port:%s\n", port);
	}
    
	int s;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }
    
    if (bind(sock_fd, result->ai_addr, result->ai_addrlen) != 0) {
        perror("bind()");
        exit(1);
    }
    
    if (listen(sock_fd, 10) != 0) {
        perror("listen()");
        exit(1);
    }
    
    struct sockaddr_in *result_addr = (struct sockaddr_in *) result->ai_addr;
	if (DEBUG){
    	printf("Listening on file descriptor %d, port %d\n", sock_fd, ntohs(result_addr->sin_port));
	}
    printf("Listening on port %s\n", port);
    
	if (DEBUG){
    	printf("Waiting for connection...\n");
	}
    int client_fd = accept(sock_fd, NULL, NULL);
	if (DEBUG){
    	printf("Connection made: client_fd=%d\n", client_fd);
	}

    
	// {recv_message}
    size_t fake_m_size = recv_message_size(client_fd);
    char* fake_m = (char *)malloc(sizeof(char) * fake_m_size); //1
    recv_message(client_fd, fake_m, fake_m_size);

	if (DEBUG){
    	printf("message:%s\n", fake_m);
	}

    
    // {get_filenames_begin} get filenames from given directory
    size_t filename_len = 1;
    char* filenames = (char*)malloc(filename_len); // 3
    *filenames='\0';
    
    DIR *d;
    struct dirent *dir;
    d = opendir(client_dir);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG){
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
    char* server_filenames = filenames;
	if (DEBUG){
    	printf("server_filenames:%s\n", server_filenames);
	}
	
	// {recv_message}
    fake_m_size = recv_message_size(client_fd);
    char* client_filenames = (char *)malloc(sizeof(char) * fake_m_size); // |client_filenames
    recv_message(client_fd, client_filenames, fake_m_size);

	if (DEBUG){
    	printf("message:%s\n", client_filenames);
	}
    
    size_t count_client_filenames = count_file_num(client_filenames);
    printf("Received a list of %zu files to compare.\n", count_client_filenames);

	// compare the filenames
	// common_filenames
  	filename_len = 1;
    char* common_filenames = (char*)malloc(filename_len); // 401
    *common_filenames='\0';
	// uncommon_filenames
  	size_t unfilename_len = 1;
    char* uncommon_filenames = (char*)malloc(unfilename_len); // 401
    *uncommon_filenames='\0';

    
    // OUTLOOP split client file token
    // [INPUT_BEGIN]
    char* input_str = client_filenames;
    // [INPUT_END]
    char dim[2] = ",";
    char *token;
    
    /* get the first token */
    token = strtok(input_str, dim);
    
    /* walk through other tokens */
    while( token != NULL ) {
		// [insert_function_piece_begin]
		if (DEBUG){
        	printf( "token_file:%s\n", token );
		}
		bool is_on_server = is_file_on_server(token, server_filenames);
		if (is_on_server == true) {
                size_t filename_size = strlen(token);
                filename_len += filename_size + 1;
                common_filenames = (char*)realloc(common_filenames, filename_len);
				if (DEBUG){
                	printf("is_on_server:%s\n", token);
				}
                strcat(common_filenames, token);
                strcat(common_filenames, ",");
		} else {
                size_t filename_size = strlen(token);
                unfilename_len += filename_size + 1;
                uncommon_filenames = (char*)realloc(uncommon_filenames, unfilename_len);
				if (DEBUG){
                	printf("is_not_on_server:%s\n", token);
				}
                strcat(uncommon_filenames, token);
                strcat(uncommon_filenames, ",");
		}
		// [insert_function_piece_end]
		input_str = input_str + (char)(strlen(token) + 1);
        token = strtok(input_str, dim);
    }
	common_filenames[strlen(common_filenames) - 1] = '\0';
	if (DEBUG){
		printf("common_filenames:%s\n", common_filenames);
		printf("uncommon_filenames:%s\n", uncommon_filenames);
	}
	
    size_t count_common_file = count_file_num(common_filenames);
    printf("%zu file(s) do not exist. Requesting remaining %zu checksums.\n", count_client_filenames - count_common_file ,count_common_file);
    printf("Generating checksums for %zu file(s)\n......\n", count_common_file);
	// [SEND] send checksum list
	send_message(client_fd, common_filenames);
    
    // 123 {CHECKSUM_BEGIN}
    size_t checksum_len= 1;
    char* checksums = (char*)malloc(checksum_len); // 3
    *checksums ='\0';
    
    
    // OUTLOOP split client file token
    // [INPUT_BEGIN]
    input_str = (char*)malloc(sizeof(char) * (strlen(common_filenames)+1)); // free111
    strcpy(input_str, common_filenames);
    
    size_t f_num = count_file_num(input_str);
    pthread_t* ids = (pthread_t*)calloc(f_num, sizeof(pthread_t));
    // [INPUT_END]
    char dim_jj[2] = ",";
    /* get the first token */
    token = strtok(input_str, dim_jj);
    
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
        //    printf("checksum_num:%zu\n", checksum_num);
        //}
        // [insert_function_piece_end]
        
        token = strtok(NULL, dim_jj);
    }
    free(input_str);
    
    //pthread_barrier_wait(&mybarrier);
    
    input_str = (char*)malloc(sizeof(char) * (strlen(common_filenames)+1)); // free111
    strcpy(input_str, common_filenames);
    // [INPUT_END]
    /* get the first token */
    token = strtok(input_str, dim_jj);
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
        
        token = strtok(NULL, dim_jj);
    }
    free(input_str);
    //pthread_barrier_destroy(&mybarrier);
    free(ids);
    
    checksums[strlen(checksums) - 1] = '\0';
    if (DEBUG){
        printf("checksums:%s\n", checksums);
    }
    // {CHECKSUM_END}
    
    
    char* server_checksums = checksums;
	if (DEBUG){
    	printf("server_checksums:%s\n", server_checksums);
	}
    printf("Done.\n");

    // {RECV_BEGIN} checksums
    size_t m_size = recv_message_size(client_fd);
    char* client_checksums = (char *)malloc(sizeof(char) * m_size); //1
    recv_message(client_fd, client_checksums, m_size);
	if (DEBUG){
    	printf("client_checksums:%s\n", client_checksums);
	}
    printf("%zu checksums received.\nComparing checksums.\n", count_common_file);
    // {RECV_END} checksums
    
    // **compare the checksums
    filename_len = 1;
    char* uncommon_checksums = (char*)malloc(filename_len); // 401
    *uncommon_checksums='\0';
    
    // OUTLOOP split client file token
    // [INPUT_BEGIN]
    input_str = client_checksums;
    // [INPUT_END]
    char dim_k[2] = ",";
    
    /* get the first token */
    token = strtok(input_str, dim_k);
    
    /* walk through other tokens */
    while( token != NULL ) {
        // [insert_function_piece_begin]
		if (DEBUG){
        	printf( "token_file:%s\n", token );
		}
        bool is_on_server = is_file_on_server(token, server_checksums);
		size_t token_size = strlen(token);
        if (is_on_server == false) {
			
			char* k_name = strtok(token, ";");
            size_t filename_size = strlen(k_name);
			strtok(NULL, ";");
            filename_len += filename_size + 1;
            uncommon_checksums = (char*)realloc(uncommon_checksums, filename_len);
            strcat(uncommon_checksums, k_name);
			if (DEBUG){
        		printf( "k_name:%s\n", k_name );
			}
            strcat(uncommon_checksums, ",");
        }
        // [insert_function_piece_end]
        input_str = input_str + (char)(token_size + 1);
        token = strtok(input_str, dim_k);
    }
    uncommon_checksums[strlen(uncommon_checksums) - 1] = '\0';
	if (DEBUG){
    	printf("uncommon_checksums:%s\n", uncommon_checksums);
	}

	// [FINAL] -> generating common file list
	filename_len = strlen(uncommon_filenames) + strlen(uncommon_checksums) + 1;
	uncommon_filenames = (char*)realloc(uncommon_filenames, filename_len);
	strcat(uncommon_filenames,uncommon_checksums);
	if (DEBUG){
		printf("uncommon_filenames:%s\n", uncommon_filenames);
	}
    
    size_t count_total_file = count_file_num(uncommon_filenames);
    printf("Requesting %zu files.\n",count_total_file);
	// [SEND] - uncommon_filenames
	send_message(client_fd, uncommon_filenames);

	// [WRITING] files to disk	
    input_str = uncommon_filenames;
    // [INPUT_END]
    char dim_f[2] = ",";
    
    /* get the first token */
    token = strtok(input_str, dim_f);
    
    /* walk through other tokens */
    while( token != NULL ) {
        // [insert_function_piece_begin]

		// [RECV_BEGIN] header
		size_t m_size = recv_message_size(client_fd);
		char* header_str = (char *)malloc(sizeof(char) * m_size); //1}
		recv_message(client_fd, header_str, m_size);
		// [RECV_END] header

		// {RECV_BEGIN} content
		m_size = recv_message_size(client_fd);
		char* content = (char *)malloc(sizeof(char) * m_size); //1
		if (DEBUG){
			printf("final_m_size:%zu\n", m_size);
		}
		recv_message(client_fd, content, m_size);
		if (DEBUG){
			printf("final_content:%s\n", content);
		}
		// {RECV_END} content


		char* fullname = mem_get_full_name(client_dir, token);

		if (token == NULL){
			if (DEBUG){
				printf("token is NULL\n");
			}
		}

		char is_written[100];
		*is_written = '\0';
		if (token != NULL && fullname != NULL){
			FILE *fp;
			fp = fopen(fullname,"w+");
			if (fp == NULL){
				strcpy(is_written, "Failed to write : Access denied");
			} else {
				strcpy(is_written, "OK");
                str_decrypt(content);
				fprintf(fp, "%s", content);
				fclose(fp);
			}
			printf("%s (%s bytes)  - received OK. written %s.\n", token, header_str, is_written);
		}
		// [SEND] confirm message
		send_message(client_fd, is_written);

		mem_free_full_name(fullname);
		free(content);
		// [insert_function_piece_end]
		token = strtok(NULL, dim_f);
    }
    printf("Finished with 0 failure\n");

	free(common_filenames);  //401
	free(uncommon_filenames);  //401
    free(fake_m); //1
	free(client_filenames); //|client_filenames
	free(filenames); //|client_filenames
    free(client_checksums);
    free(uncommon_checksums);
}
