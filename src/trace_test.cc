
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "liburing.h"

#define QD	32
#define BUF_SIZE 4096 

#define KB (1024LL)
#define MB (1024 * 1024LL)
#define GB (1024 * 1024 * 1024LL)

#define MAX_IOSIZE (10 * MB)

#define USER_SPACE_LEN (10 * GB)

int main(int argc, char *argv[])
{
	struct io_uring ring;
	int i, fd, ret, pending, done;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	//struct iovec *iovecs;
	struct stat sb;
	ssize_t fsize;
	off_t offset;
	void *buf;

	/*if (argc < 2) {
		printf("%s: file\n", argv[0]);
		return 1;
	}*/

	ret = io_uring_queue_init(QD, &ring, 0);
	if (ret < 0) {
		fprintf(stderr, "queue_init: %s\n", strerror(-ret));
		return 1;
	}

	//fd = open(argv[1], O_RDONLY | O_DIRECT); 
	//fd = open("/home/femu/do.sh", O_RDWR | 040000); 
	fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
	printf("%d\n",fd);


	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (fstat(fd, &sb) < 0) {
		perror("fstat");
		return 1;
	}

	//return 0;

	fsize = 0;
	struct iovec *iovecs =(struct iovec*) calloc(QD, sizeof(struct iovec));

        // buf = malloc(BUF_SIZE);
		posix_memalign((void **)&buf, 512, BUF_SIZE);
		
		iovecs[i].iov_base = buf;
		iovecs[i].iov_len = 4096;
		fsize += 4096;

	offset = 7965188096;
    //i = 0;
	//do {
		sqe = io_uring_get_sqe(&ring);
		//if (!sqe)
		//	break;
		io_uring_prep_readv(sqe, fd, &iovecs[0], 1, offset);
		// sqe->added_info=29;
		// sqe->user_data = 29;
		offset += iovecs[0].iov_len;
	//	i++;
		//if (offset >= sb.st_size)
		//	break;
	//} while (1);

	ret = io_uring_submit(&ring);
	if (ret < 0) {
		fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
		return 1;
	} else if (ret != 1) {
		fprintf(stderr, "io_uring_submit submitted less %d\n", ret);
		return 1;
	}

	done = 0;
	pending = ret;
	fsize = 0;
	for (i = 0; i < 1; i++) {
		ret = io_uring_wait_cqe(&ring, &cqe);
		//printf("%d\n",ret);
		printf("%d\n",cqe->res);
		//if (ret < 0) {
		//	fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
		//	return 1;
		//}

		done++;
		ret = 0;
		if (cqe->res != 4096 && cqe->res + fsize != sb.st_size) {
			fprintf(stderr, "ret=%d, wanted 4096\n", cqe->res);
			ret = 1;
		}
		fsize += cqe->res;
		io_uring_cqe_seen(&ring, cqe);
	//	if (ret)
	//		break;
	}

	printf("Submitted=%d, completed=%d, bytes=%lu\n", pending, done,
						(unsigned long) fsize);
	printf("cqe->usr_data:%llu\n",cqe->user_data);
	close(fd);
	io_uring_queue_exit(&ring);
	return 0;
}
