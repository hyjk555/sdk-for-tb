#include <linux/mtd/nand.h>
#include <linux/mtd/mtd.h>
#include <nand.h>
#include <ingenic_nand_mgr/nand_param.h>
#include "cloner.h"

#ifdef CONFIG_JZ_NAND_MGR
int nand_program(struct cloner *cloner)
{
	u32 startaddr = cloner->cmd->write.partation + (cloner->cmd->write.offset);
	u32 length = cloner->cmd->write.length;
	void *databuf = (void *)cloner->write_req->buf;

	printf("=========++++++++++++>   NAND PROGRAM:startaddr = %d P offset = %d P length = %d \n",startaddr,cloner->cmd->write.offset,length);
	do_nand_request(startaddr, databuf, length,cloner->cmd->write.offset);

	return 0;
}
#endif

#ifdef CONFIG_MTD_NAND_JZ
extern char *get_expart_name(int offset);
extern char *get_part_name(int offset);
extern int set_current_part(char *name);
extern int need_change_part(char *name);

extern int mtd_nand_probe_burner(PartitionInfo *pinfo, nand_flash_param *nand_params,
		int nr_nand_args, int eraseall, void ** title_fill, int *size);

int nand_mtd_ubi_program(struct cloner *cloner)
{
	u32 full_size = cloner->full_size;
	u32 length = cloner->cmd->write.length;
	char command[128];
	char *partation;
	char *volume;
	void *databuf = (void *)cloner->write_req->buf;
	int ret = 0;
	int offset = cloner->cmd->write.partation;
	char *part_name = NULL;

	if (cloner->full_size_remainder == 0)
		return 0;

	if (cloner->full_size) {
		part_name = get_part_name(offset);
		if (need_change_part(part_name)) {
			memset(command, 0, 128);
			sprintf(command, "ubi part %s", part_name);
			printf("%s\n", command);
			ret = run_command(command, 0);
			if (ret) {
				printf("error...\n");
				return ret;
			}
			printf("ok....\n");
			set_current_part(part_name);
			cloner->last_offset = 0;
			cloner->last_offset_avail = 0;
		}
	}

	printf("cloner->last_offset_avail = %d,  cloner->last_offset = %x cloner->cmd->write.offset = %x\n",
			cloner->last_offset_avail,
			cloner->last_offset,
			cloner->cmd->write.offset);
	if (cloner->last_offset_avail &&
			cloner->last_offset ==
			cloner->cmd->write.offset)
		return 0;

	memset(command, 0, 128);
	volume = get_expart_name(offset);
	if (full_size && full_size <= length) {
		length = full_size;
		sprintf(command, "ubi write 0x%x %s 0x%x", (unsigned)databuf, volume, length);
	} else if (full_size) {
		sprintf(command, "ubi write.part 0x%x %s 0x%x 0x%x",
				(unsigned)databuf, volume, length, full_size);
	} else {
		sprintf(command, "ubi write.part 0x%x %s 0x%x",
				(unsigned)databuf, volume, length);
	}
	cloner->full_size_remainder -= length;
	if (cloner->full_size_remainder < 0) cloner->full_size_remainder = 0;
	printf("%s (full_size_remainder reserved = %d)\n", command, cloner->full_size_remainder);
	ret = run_command(command, 0);
	if (ret) {
		printf("...error\n");
	} else {
		printf("...ok\n");
		cloner->last_offset_avail = 1;
		cloner->last_offset = cloner->cmd->write.offset;
	}
	cloner->full_size = 0;
	return ret;
}

extern void* get_params_addr(void);
extern int get_nand_pagesize(void);
int nand_mtd_raw_program(struct cloner *cloner)
{
	u32 length = cloner->cmd->write.length;
	void *databuf = (void *)cloner->write_req->buf;
	u32 startaddr = cloner->cmd->write.partation + (cloner->cmd->write.offset);
	void *parambuf = NULL;
	char command[128];
	int ret = 0;
	int page_size = get_nand_pagesize();

	if (page_size < 0)
		return -1;

	if (startaddr == 0) {
		if (length < CONFIG_SPL_PAD_TO)
			return -1;
	} else if (startaddr < CONFIG_SPL_PAD_TO)
		return -1;

	if (!cloner->args->nand_erase) {
		int erase_len = length;
		memset(command, 0 , 128);
		if (startaddr == 0)
			erase_len = (erase_len - CONFIG_SPL_PAD_TO + page_size * 128 * 8);
		sprintf(command, "nand erase 0x%x 0x%x", startaddr, erase_len);
		printf("%s\n", command);
		ret = run_command(command, 0);
		if (ret) goto out;
	}

	if (startaddr == 0) {
		memcpy(databuf, cloner->spl_title, cloner->spl_title_sz);
		memset(command, 0 , 128);
		sprintf(command, "writespl 0x%x 0x%x", (unsigned)databuf, 6);
		printf("%s\n", command);
		ret = run_command(command, 0);
		if (ret)
			goto out;
		parambuf = get_params_addr();
		memset(command, 0 , 128);
		sprintf(command, "writespl %p 0x%x 0x%x", parambuf, 6, 2);
		printf("%s\n", command);
		ret = run_command(command, 0);
		if (ret)
			goto out;

		startaddr += (page_size * 128 * 8);
		databuf += CONFIG_SPL_PAD_TO;
		length -= CONFIG_SPL_PAD_TO;
		if (ret)
			goto out;
		printf("...ok\n");
	}

	memset(command, 0 , 128);
	sprintf(command, "nand write.skip 0x%x 0x%x 0x%x", (unsigned)databuf, startaddr, length);
	printf("%s\n", command);
	ret = run_command(command, 0);
	if (ret) goto out;
	cloner->full_size = 0;
	printf("...ok\n");
	return 0;
out:
	printf("...error\n");
	return ret;
}
#endif
