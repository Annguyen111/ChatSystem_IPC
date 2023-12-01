# ChatSystem_IPC
# Hệ thống chat kết hợp semmaphore
## Cài đặt và chạy
- Tải về source code từ [GitHub]
- Compile file bằng lệnh: gcc -o [tên mới] [tên file.c] 
- Chạy file bằng lệnh: ./[tên mới]
## Mô tả các phần trong project
1. **Client**: Là module client, có nhiệm vụ giao tiếp với server qua socket
2. **Server**: Là module server, có nhiệm vụ xử lý request từ Client
## Chức năng:
- Chat nhóm
- Chat 1 - 1
- Gửi file qua lại 
