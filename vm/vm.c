/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
//pg_round_down() 함수를 위해 추가
#include "threads/mmu.h"

//vm_entry를 위해 추가
#include "userprog/process.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. 
 * 전달된 type으로 초기화되지 않은 페이지 생성
 * */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *p = (struct page *)malloc(sizeof(struct page));
		if(p == NULL) {
			goto err;
		}
		bool (*page_initializer)(struct page *, enum vm_type, void *); //함수 포인터
		switch (VM_TYPE(type)) { //타입에 맞는 초기화 함수 지정
			case VM_ANON :
				page_initializer = anon_initializer;
				break;
			case VM_FILE :
				page_initializer = file_backed_initializer;
				break;
			default :
				NOT_REACHED();
				break;
			}
		uninit_new(p, upage, init, type, aux, page_initializer); //VM_UNINIT 타입으로 페이지 생성
		p->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. 
 * 추가 페이지 테이블에서 va에 해당하는 페이지 찾는 함수
 */
struct page * spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = (struct page *)malloc(sizeof(struct page));
	page->va = pg_round_down(va);

	//va와 동일한 해시 검색
	struct hash_elem *e = hash_find(&spt->hash_table, &page->hash_elem);
	free(page); //사용을 완료한 페이지 메모리 해제하기
	if (e == NULL) { // 없을 경우
		return NULL;
	}
	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. 
 * 추가 페이지 테이블에 페이지 삽입하는 함수
 * 성공할 경우 : True / 실패할 경우 : False
 */
bool spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_insert(&spt->hash_table, &page->hash_elem);
	if(e == NULL) { //성공했을 경우
		succ = true;
	}
	return succ;
}

//SPT에서 페이지 제거하는 함수
void spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	struct hash_elem *e = hash_delete(&spt->hash_table, &page->hash_elem);
	if(e == NULL) {
		return;
	}
	vm_dealloc_page(page);
	return;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	// struct thread *curr = thread_current();

	// lock_acquire(&frame_table_lock);
	// for (struct list_elem *start = list_begin(&frame_table); start != list_end(&frame_table); start = list_next(start)) {
	// 	victim = list_entry(start, struct frame, frame_elem);
	// 	if(victim->page == NULL) {
	// 		lock_release(&frame_table_lock);
	// 		return victim;
	// 	}
	// 	if (pml4_is_accessed(curr->pml4, victim->page->va)) {
	// 		pml4_set_accessed(curr->pml4, victim->page->va, 0);
	// 	}
	// 	else {
	// 		lock_release(&frame_table_lock);
	// 		return victim;
	// 	}
	// }
	// lock_release(&frame_table_lock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// if(victim->page) {
	// 	swap_out(victim->page);
	// }
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	//사용자 풀에서 페이지를 할당받기 - 할당받은 물리 메모리 주소 반환
	void *addr = palloc_get_page(PAL_USER);
	if(addr == NULL) {
		PANIC("vm_get_frame()");
	}

	frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = addr;
	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page(VM_ANON | VM_MARKER_0, pg_round_down(addr), 1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (addr == NULL || (is_kernel_vaddr(addr) && user)) { //올바르지 못한 주소일 경우
		return false;
	}
	if (not_present) { //접근하려는 페이지가 물리 메모리에 존재하지 않을 경우
		void *rsp = f->rsp;
		if(((USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr) || (USER_STACK - (1 << 20) <= rsp && rsp == addr)) && addr <= USER_STACK) {
			vm_stack_growth(addr);
		}

		page = spt_find_page(spt, addr);
		if(page == NULL) {
			return false;
		}
		if (write && (!page->writable)) { //권한이 없는데 쓰려고 하는 경우
			return false;
		}
		return vm_do_claim_page(page);
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if(page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	bool result = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
	if(result == false) {
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table 
 * 추가 페이지 테이블 초기화하는 함수
 */
void supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst 
 * 자식이 부모의 실행 컨텍스트를 상속해야 할 때 사용 - fork()
 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i; //해시 테이블 내의 위치
	hash_first(&i, &src->hash_table); //i를 해시의 첫번째 요소를 가리키도록 초기화
	while (hash_next(&i)) { // 해시의 다음 요소가 있을 때까지 반복
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *va = src_page->va;
		bool writable = src_page->writable;

		if(type == VM_UNINIT) { //초기화되지 않은 페이지인 경우
			vm_alloc_page_with_initializer(page_get_type(src_page), va, writable, src_page->uninit.init, src_page->uninit.aux);
		}
		else if(type == VM_FILE) { //파일 타입일 경우
			struct vm_entry *vme = (struct vm_entry *)malloc(sizeof(struct vm_entry));
			vme->f = src_page->file.file;
			vme->offset = src_page->file.offset;
			vme->read_bytes = src_page->file.read_bytes;
			vme->zero_bytes = src_page->file.zero_bytes;

			if(!vm_alloc_page_with_initializer(type, va,writable, NULL, vme)) {
				return false;
			}
			struct page *page = spt_find_page(dst, va);
			file_backed_initializer(page, type, NULL);
			page->frame = src_page->frame;
			pml4_set_page(thread_current()->pml4, page->va, src_page->frame->kva, src_page->writable);
		}
		else { //익명 페이지일 경우
			if(!vm_alloc_page(type, va, writable)) {
				return false;
			}
			if(!vm_claim_page(va)) {
				return false;
			}
			struct page *dst_page = spt_find_page(dst, va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table 
 * process_exit() : 프로세스 종료시 호출
 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_table, hash_page_destroy);
}

//들어온 요소 e의 가상 주소 값을 unsigned int형 범위의 값으로 변경
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

//가상 주소를 비교하여 반환
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);
	return a->va < b->va;
}

void hash_page_destroy(struct hash_elem *e, void *aux) {
	struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}