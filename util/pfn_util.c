#include "util.h"
#include "pfn_util.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>

uint64_t get_physical_frame_number(uint64_t vpn) {
	// printf("check vpn = 0x%lx\n", vpn);
	int i;
	// int pid = getpid();
	// string pagemap_path = "/proc/"+to_string(getpid())+"/pagemap";
	// FILE *f = fopen(pagemap_path.c_str(), "rb");

	/* Open the pagemap file for the current process */
	FILE *f = fopen("/proc/self/pagemap", "rb");

	if(f==NULL){
			printf("Error! Cannot open /proc/self/pagemap: %s\n", strerror(errno));
			abort();
	}

	uint64_t file_offset = vpn * PAGEMAP_ENTRY_SIZE;
	int ret = fseek(f, file_offset, SEEK_SET);
	if(ret !=0 ){
		printf("Error in fseek\n");
		abort();
	}

	uint64_t read_val = 0;

	ret = fread(&read_val, PAGEMAP_ENTRY_SIZE, 1, f);
	fclose(f);
	if(ret <0 ){
		printf("Error in fread\n");
		abort();
	}

	// printf("reav_val = 0x%lx\n", read_val);
	if(GET_BIT(read_val, 63)){
		// printf("VPN: 0x%lx; PFN: 0x%llx\n",
		// 	vpn, (unsigned long long) GET_PFN(read_val));
		return GET_PFN(read_val);
	}else
		printf("Page not present\n");

	if(GET_BIT(read_val, 62))
		printf("Page swapped\n");


	return 0;

}