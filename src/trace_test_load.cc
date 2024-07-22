#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <liburing.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <sstream>

using namespace std;

#define RING_QD (16)
#define RING_FLAG (IORING_SETUP_IOPOLL)

#define RING_FLAG_TEMP 0

#define KB (1024LL)
#define MB (1024 * 1024LL)
#define GB (1024 * 1024 * 1024LL)

#define MAX_IOSIZE (10 * MB)

#define USER_SPACE_LEN (10 * GB)

struct io_request {
  int fd;
  uint64_t offset;
  uint64_t len;
  char *buf;
  int added_info;
  bool read;
};

struct iovec *iovecs;
int req_cnt = 0;
int sub_cnt = 0;

int cnt;

long long pri_cnt, total_cnt;
long long total_pri_val, total_val;

int bin[1000];

template <class Container>
void str_split(const string &str, Container &cont, const string &delims = " ") {
  size_t current, previous = 0;
  current = str.find_first_of(delims);
  while (current != string::npos) {
    cont.push_back(str.substr(previous, current - previous));
    previous = current + 1;
    current = str.find_first_of(delims, previous);
  }
  cont.push_back(str.substr(previous, current - previous));
}

void submit_io(struct io_uring *ring, struct io_request *req) {
  /*
  //打印IO请求
  cout << "offset: " << req->offset << " length: " << req->len << " "
       << "added_info: " << req->added_info << " read: " << req->read << endl;
  */
  
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);       // 得到 sq 中一个空闲的请求项（即 tail 处的 entry ，随后 tail += 1）
                                                           // 即先占着坑，先把它申请下来，随后再直接往队列里填请求的具体信息
                                                           // 而不是先生成 sqe ，然后再写入 sq 中
  io_uring_prep_rw(req->read ? IORING_OP_READV : IORING_OP_WRITEV, sqe, req->fd,
                   &iovecs[0], 1, req->offset);       // 填充 sqe （其实是直接往 sq 里写了，sqe 是个指针，指向了 sq 中的一项）

  sqe->added_info = req->added_info;           // 为请求 added_info 字段赋值

  req_cnt ++;
  
  // printf("sqe->user_data:%llu\n", sqe->user_data);
  if(req_cnt >= RING_QD){
    io_uring_submit(ring);                                   // 请求详细信息填充完毕后，提交请求
    sub_cnt += req_cnt;
    req_cnt = 0;
  }
}

void wait_completion(struct io_uring *ring) {
  struct io_uring_cqe *cqe;
  io_uring_wait_cqe(ring, &cqe);       // 取出头部 cqe
  // printf("cqe->user_data:%llu\n", cqe->user_data);
  //打印res

  /*
  cout << "res: " << cqe->res << endl;
  cout << "user_data: " << cqe->user_data << endl << endl;
  */
  
  io_uring_cqe_seen(ring, cqe);       // 更新 cq 队列（因为头部 cqe 已被取出）

  if(cqe->user_data >= 10001 && cqe->user_data <= 10010 ){
    pri_cnt ++;
    total_pri_val += cqe->res;

    // cout << cqe->res << endl;
    int idx = (cqe->res) / 1000;

    if(idx >= 0 && idx < 1000) bin[idx] ++;
  }

  total_cnt ++;
  total_val += cqe->res;

  sub_cnt --;
}

int main() {
  struct io_uring ring;

  io_uring_queue_init(RING_QD, &ring, RING_FLAG);     // RING_FLAG : IORING_SETUP_IOPOLL
                                                      // IORING_SETUP_IOPOLL 可以让内核采用 Polling 的模式收割 Block 层的请求
  
  // Open your file here
  // int fd = open("/dev/nvme0n1", O_RDWR , 0644);
  int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  // Allocate buffer for IO
  // char *buffer = new char[MAX_IOSIZE];
  char* buffer;
  posix_memalign((void **)&buffer, 512, MAX_IOSIZE);
  if (!buffer) {
    std::cerr << "Failed to allocate buffer" << std::endl;
    return 1;
  }
  string line;
  ifstream file("../Trace/output.txt");
  if (!file.is_open()) {
    cerr << "Failed to open trace file." << endl;
    return 1;
  }
  while (getline(file, line)) {
    if (line.empty())
      continue;

    vector<string> lineSplit;
    str_split(line, lineSplit, " ");
    uint64_t offset = atoll(lineSplit[2].c_str());
    uint64_t length = atoll(lineSplit[4].c_str());
    int added_info = atoll(lineSplit[0].c_str());
    if (length > 10 * MB) {
      continue;
    }
    offset = offset % USER_SPACE_LEN;
    offset = offset / (4 * KB) * (4 * KB);

    // cout << "offset: " << offset << " length: " << length << endl;

    iovecs =(struct iovec*) calloc(RING_QD, sizeof(struct iovec));

    iovecs[0].iov_base = buffer;
		iovecs[0].iov_len = length;

    if (lineSplit[1] == "R") {
      // Read operation
      io_request req = {fd, offset, length, buffer,added_info, true};
      submit_io(&ring, &req);
      while(sub_cnt) wait_completion(&ring);
    } else if (lineSplit[1] == "W") {
      // Write operation
      //  Fill buffer with random data
      for (int i = 0; i < length; i++) {
        buffer[i] = rand() % 256;
      }
      io_request req = {fd, offset, length, buffer, added_info, false};
      submit_io(&ring, &req);
      while(sub_cnt) wait_completion(&ring);
    }

    cnt ++;
    if(cnt >= 500000) break;
  }

  for(int i = 0;i <= 300;i ++) cout << bin[i] << endl;

  cout << total_pri_val / pri_cnt << endl;
  cout << (total_val - total_pri_val) / (total_cnt - pri_cnt) << endl;
  cout << total_val / total_cnt << endl;

  cout << "1 - 10 : " << total_pri_val / pri_cnt << endl;
  cout << "not in 1 - 10 : " << (total_val - total_pri_val) / (total_cnt - pri_cnt) << endl;
  cout << "total : " << total_val / total_cnt << endl;

  // Cleanup
  delete[] buffer;
  close(fd);
  io_uring_queue_exit(&ring);

  return 0;
}
