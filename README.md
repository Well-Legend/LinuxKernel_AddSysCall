Linux Operating System Project 1
===
Download Kernel Source
---
```bash
# 進入 root 模式

sudo su

# 下載kernel

wget -P ~/ https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.137.tar.xz
```
輸入指令時請注意所處位置，他會下載到當前所處資料夾

### 解壓縮
```bash
# 把檔案解壓縮到 /usr/src 目錄底下

tar -xvf linux-5.15.137.tar.xz -C /usr/src
```

### 安裝相關套件
```bash
sudo apt update && sudo apt upgrade -y

# 這個套件可以幫我們 build 出 kernel-pakage

sudo apt install build-essential libncurses-dev libssl-dev libelf-dev bison flex -y
```

### 安裝vim
```bash
sudo apt install vim -y
```

### 清除安裝的package
```bash
sudo apt clean && sudo apt autoremove -y
```
---

Question 1
---
In this part, we use malloc() to allocate memory. At first, the virtual addresses (VAs) are reserved, but they do not yet have corresponding physical addresses (PAs).
Only when the program writes to a page does the operating system actually allocate a physical page.
This mechanism, where physical memory is allocated only on first access, is called lazy allocation.

### 專案說明
在現代作業系統中，每個執行中的程式（process）都擁有獨立的虛擬位址空間。
CPU與作業系統透過**頁表** (**page table**) 將**虛擬位址**（**Virtual Address, VA**）轉換為**實體位址**（**Physical Address, PA**）。

此外，一個程式（process）底下可以包含多個執行緒（thread），這些執行緒共享同一個虛擬位址空間，卻各自擁有獨立的核心堆疊（kernel stack）與執行緒控制區塊（例如 Linux 中的 task_struct）。作業系統必須透過這些資料結構來管理排程、記憶體與權限，並藉由 mm_struct 及其頁目錄（pgd）來描述整個行程的位址空間。

本實驗的目標是實作2個**新的 Linux system call**:   
1. 用於**查詢給定虛擬位址目前對應的實體位址**，並觀察記憶體的 lazy allocation 行為。  

2. 則是用於查詢**目前呼叫該 system call 的執行緒**在核心中的關鍵資訊，包括：**執行緒的 pid、所屬行程的 tgid、對應的 process descriptor（task_struct）位址、核心模式堆疊起始位址，以及該行程所使用之頁目錄（pgd）位址**。透過撰寫單執行緒與多執行緒的測試程式並呼叫此 system call，實驗將引導我們觀察並驗證：同一個 process 中不同 thread 的 pid/tgid 關係、各自獨立的 kernel stack 與 task_struct 位址，以及共享的虛擬位址空間（pgd），從而更加具體地理解 Linux 核心內部對 process 與 thread 的實作方式  

### 實作
- **新增syscall**  
	```bash
	# 把目錄轉到剛剛解壓縮完的 kernel 檔案夾
	
	cd /usr/src/linux-5.15.137
	
	# 在裡面創建一個名叫 project1 的資料夾
	
	mkdir project1
	
	# 把目錄轉到 project1 資料夾
	
	cd project1
	```
 	***
 
- **建立第一個system call的新檔案**
	```bash
	vim my_get_physical_addresses.c
	```
	並填入 code
	```c
	#include <linux/kernel.h>
	#include <linux/syscalls.h>
	#include <linux/mm.h>
	#include <linux/mmap_lock.h>
	#include <linux/sched.h>
	#include <asm/pgtable.h>
	
	SYSCALL_DEFINE1(my_get_physical_addresses, unsigned long, vaddr)
	{
	    struct mm_struct *mm = current->mm;
	    unsigned long addr = vaddr, pa = 0;
	    pgd_t *pgd; p4d_t *p4d; pud_t *pud; pmd_t *pmd; pte_t *pte;
	
	    if (!mm) return 0;
	    if (addr >= TASK_SIZE_MAX) return 0;
	
	    mmap_read_lock(mm);
	
	    pgd = pgd_offset(mm, addr);
	    if (pgd_none(*pgd) || pgd_bad(*pgd)) goto out;
	    p4d = p4d_offset(pgd, addr);
	    if (p4d_none(*p4d) || p4d_bad(*p4d)) goto out;
	    pud = pud_offset(p4d, addr);
	    if (pud_none(*pud) || pud_bad(*pud)) goto out;
	    pmd = pmd_offset(pud, addr);
	    if (pmd_none(*pmd) || pmd_bad(*pmd)) goto out;
	
	    pte = pte_offset_map(pmd, addr);
	    if (!pte) goto out;
	    if (!pte_present(*pte)) { pte_unmap(pte); goto out; }
	
	    pa = ((unsigned long)pte_pfn(*pte) << PAGE_SHIFT) |
	         (addr & (PAGE_SIZE - 1));
	
	    pte_unmap(pte);
	
	out:
	    mmap_read_unlock(mm);
	    return pa;
	}
	```
	***

- **建立第二個system call的新檔案**
	```bash
	vim my_get_thread_kernel_info.c
	```
	並填入 code
	```c
	#include <linux/kernel.h>
	#include <linux/syscalls.h>
	#include <linux/sched.h>    // current, task_struct
	#include <linux/mm.h>       // mm_struct
	#include <linux/uaccess.h>  // copy_to_user
	#include <asm/current.h>

	struct my_thread_info_record {
		unsigned long pid;
		unsigned long tgid;
		void *process_descriptor_address;
		void *kernel_mode_stack_address;
		void *pgd_table_address;
	};

	SYSCALL_DEFINE1(my_get_thread_kernel_info, void __user *, user_buf)
	{
		struct my_thread_info_record info;
		struct task_struct *tsk = current;
		struct mm_struct *mm;

		if (!user_buf)
			return 0;

		/* 1. pid / tgid */
		info.pid  = (unsigned long)tsk->pid;
		info.tgid = (unsigned long)tsk->tgid;

		/* 2. process descriptor 位址 (task_struct*) */
		info.process_descriptor_address = (void *)tsk;

		/* 3. kernel 模式 stack */
		info.kernel_mode_stack_address = (void *)tsk->stack;

		/* 4. pgd table 位址 */
		mm = tsk->mm ? tsk->mm : tsk->active_mm;
		if (mm)
			info.pgd_table_address = (void *)mm->pgd;
		else
			info.pgd_table_address = NULL;

		/* 5. 丟回 user space */
		if (copy_to_user(user_buf, &info, sizeof(info)))
			return 0;       // 拷貝失敗

		return 1;           // 成功
	}
	```
	***

- **在 project1 資料夾中建立 Makefile**
	```bash
	vim Makefile
	```
 
	並填入 code

	```bash
	# 將 my_get_physical_addresses.o 和 my_get_thread_kernel_info.o 編入 kernel
	
	obj-y := my_get_physical_addresses.o my_get_thread_kernel_info.o 
	```
 	Makefile功能 : 告訴 make 如何編譯這些檔案  
	Makefile 的主要目的是讓 make 工具知道如何根據源代碼的變化來更新和編譯目標檔案，以及確保這些操作按照正確的順序執行。

	***
- **編輯原系統中的Makefile**  
  ```bash
  cd ..

  nano Makefile
  ```
  
  並找到這一行
  
  ```bash
  core-y			+= kernel/ certs/ mm/ fs/ ipc/ security/ crypto/
  ```
  
  並在其後加上 `project1/` 即
  
  ```bash
  core-y			+= kernel/ certs/ mm/ fs/ ipc/ security/ crypto/ project1/
  ```

	***
- **將系統呼叫對應的函數加入到系統呼叫的標頭檔中**  
  ```bash
  nano include/linux/syscalls.h
  ```

  並在`#endif`前加上
  ```bash
  asmlinkage long sys_my_get_physical_addresses(unsigned long __user *usr_ptr);

  asmlinkage long sys_my_get_thread_kernel_info(void __user *user_buf);
  ```

	當 assembly code 呼叫 C function，並且是以 stack 方式傳參數（parameter）時，在 C function 的 prototype 前面就要加上 "asmlinkage"。

	***
- **將syscall加入到Kernel的syscall table**  
  ```bash
  nano arch/x86/entry/syscalls/syscall_64.tbl
  ```
  拉至其底部。會發現一系列 x32 系統呼叫。滾動到其上方的section。並插入
  ```bash
  449	common	my_get_physical_addresses	sys_my_get_physical_addresses

  450	common	my_get_thread_kernel_info	sys_my_get_thread_kernel_info
  ```
  使用 Tab 來表示空格，勿使用空白鍵!!

	***
- **編譯設定**
  ```bash
  make clean
  cd
  cd /usr/src/linux-5.15.137/
  cp -v /boot/config-$(uname -r) .config
  make localmodconfig
  ```
  ```bash
  scripts/config --disable SYSTEM_TRUSTED_KEYS
  scripts/config --disable SYSTEM_REVOCATION_KEYS
  scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
  scripts/config --set-str CONFIG_SYSTEM_REVOCATION_KEYS ""
  ```
  
  ***
- **編譯Kernel並重開機**
  ```bash
  make -j$(nproc)
  sudo make modules_install
  sudo make install
  sudo update-grub
  sudo reboot

  ***
- **檢查版本**
  ```bash
  uname -rs
  ```
  
### 測試驗證
```bash
# Virtual Address 轉 Physical Address
gcc -O2 -o test_address_trans test_address_trans.c
./test_address_trans.c

# 呼叫該 system call 的執行緒 - single-thread program
gcc single.c -o single
./single

# 呼叫該 system call 的執行緒 - multi-thread program
gcc multi.c -o multi -lpthread
./multi
```

***
Question 2
---
In this part, you need to convert a C system call into a shared library so that it can be called from Python using ctypes library.

### 專案說明
在 Question 1 中，我們已經在 Linux Kernel 5.15.137 上實作完成一個自訂的 system call：
```bash
unsigned long my_get_physical_addresses(void *va)
```
可用於查詢虛擬位址 (VA) 與實體位址 (PA) 的映射關係。

本題的目標是將該 system call 轉換為 動態共享函式庫 (.so)，使其能夠被 Python (ctypes) 直接呼叫，並透過高階語言觀察記憶體配置行為，包括：  

- heap 區 (malloc/sbrk) 成長過程
- lazy allocation 與 page fault 配頁
---
### 實驗目標
1. 實作一個 C wrapper，將 system call 封裝為可供 Python 呼叫的共享函式。
2. 於 Python 中使用 ctypes 載入 .so 並呼叫該函式。
3. 透過 Python 程式分配記憶體、檢查對應的物理位址變化，觀察 lazy allocation 行為。

---
### 實驗環境
| 項目 | 設定值|
| :-- | :-- |
| 作業系統  | Ubuntu 22.04 LTS |
| Kernel 版本  | Linux 5.15.137 |
| System call number  | 449 |
| 測試語言  | Python 3.12 |
| 介接方法  | ctypes + Shared Library (.so) |

***
### C Wrapper 實作
- **建立cwrapper.c**
```bash
sudo su

cd /usr/src/linux-5.15.137/project1

vim cwrapper.c
```
```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdint.h>

#ifndef SYS_GET_PHY
#define SYS_GET_PHY 449
#endif


__attribute__((visibility("default")))
unsigned long my_get_physical_addresses(void *va) {
    long ret = syscall(SYS_GET_PHY, va);
    if (ret < 0) return 0;         
    return (unsigned long)ret;
}

```
***
- **編譯成共享函式庫**
```bash
gcc -Wall -O2 -fPIC -shared -o lib_my_get_phy.so cwrapper.c
```
成功後會產生：
```bash
lib_my_get_phy.so
```

***
- **測試驗證**
  
回到原本檔案資料夾中執行測試程式
```bash
python3 test_q2.py
```

---
## 結果分析
| 現象 | 解釋 |
| :-- | :-- |
| 只有部分頁面有 PA  | `malloc()` 或 `create_string_buffer()` 內部清零會觸碰前幾頁，導致立即分配實體頁。 |
| 其餘頁面顯示「未分配」  | 	屬於尚未被觸碰的虛擬頁面，lazy allocation 尚未觸發。 |
| program break 成長  | 代表 heap 空間擴張，對應 sbrk 的行為。 |


***
Question 3
---
In this part, you need to define new data type and write a new system call int my_get_thread_kernel_info(void *) so that a thread can use the new system call to collect some information of a thread.  
The return value of this system call is an integer. 0 means that an error occurs when executing this system call. A non-zero value means that the system is executed successfully. You can also utilize the return value to tranfer information you need.

### 專案說明

```bash
int my_get_thread_kernel_info(void *)
```
