#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

void argument_stack(char **argv, int argc, void **rsp);
struct thread *get_child_process(tid_t child_tid);

// 파일 디스크립터를 위한 함수
int process_add_file(struct file *f);
void process_close_file(int fd);
struct file *process_get_file(int fd);

//lazy_load_segment에 넘겨줄 인자들
struct vm_entry {
	struct file *f;			/* 가상 주소와 매핑된 파일 */
	off_t offset;			/* 읽어야할 파일 offset */
	uint64_t read_bytes;	/* 가상 페이지에 쓰여져 있는 데이터 크기 */
	uint64_t zero_bytes;	/* 0으로 채울 남은 페이지 byte */
};

#endif /* userprog/process.h */
