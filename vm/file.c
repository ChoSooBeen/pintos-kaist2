/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

//vm_entry를 위한 헤더파일 추가
#include "userprog/process.h"

//가상 주소를 위한 헤더파일 추가
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct vm_entry *vme = (struct vm_entry *)page->uninit.aux;
	file_page->file = vme->f;
	file_page->offset = vme->offset;
	file_page->read_bytes = vme->read_bytes;
	file_page->zero_bytes = vme->zero_bytes;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if(pml4_is_dirty(thread_current()->pml4, page->va)) { //dirty bit = 1일 경우 swap out 가능
		//변경사항을 파일에 저장하기
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
		//dirty bit = 0
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	struct file *f = file_reopen(file);
	void *start_addr = addr;
	int total_page_count;
	if(length <= PGSIZE) { //현재 매핑하려는 길이가 페이지 사이즈보다 작을 경우
		total_page_count = 1; //한 개의 페이지에 모두 들어간다.
	}
	else { //하나 이상의 페이지가 필요할 경우
		if(length % PGSIZE != 0) {  //나누어떨어지지 않을 경우 페이지 1개 더 필요하다.
			total_page_count = length / PGSIZE + 1;
		}
		else {
			total_page_count = length / PGSIZE;
		}
	}

	size_t read_bytes = file_length(f) < length ? file_length(f) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(addr) == 0);
    ASSERT(offset % PGSIZE == 0);

	while(read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct vm_entry *vme = (struct vm_entry *)malloc(sizeof(struct vm_entry));
		vme->f = f;
		vme->offset = offset;
		vme->read_bytes = page_read_bytes;
		vme->zero_bytes = page_zero_bytes;

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, vme)) {
			file_close(f);
			return NULL;
		}

		struct page *p = spt_find_page(&thread_current()->spt, start_addr);
		p->mapped_page_count = total_page_count;

		read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
        offset += page_read_bytes;
	}
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *p = spt_find_page(spt, addr);
	int count = p->mapped_page_count;
	for (int i = 0; i < count; i++) {
		if(p) {
			file_backed_destroy(p);
		}
		addr += PGSIZE;
		p = spt_find_page(spt, addr);
	}
}