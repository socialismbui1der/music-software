#include"util.h"
#include"common.h"
#include<netinet/in.h>
#include<sys/socket.h>
#include <iconv.h>
#include<string.h>
#include"easylogging.h"

void util::sendres(int m_sockfd,int type,const char* msg){
    response_msg res;
    res.header.msg_type=type;
    int msg_len=strlen(msg);
    res.header.msg_len=msg_len;
    strncpy(res.msg,msg,res.header.msg_len);
    res.header.msg_len = htonl(res.header.msg_len);
    res.header.msg_type = htonl(res.header.msg_type);
    ssize_t len=sizeof(res.header)+msg_len;
    int totalsent=0;
    while(totalsent<len){
        int bytes_sent=send(m_sockfd,reinterpret_cast<char *>(&res),len,0);
        if (bytes_sent == -1) {
            LOG(WARNING)<<"send fail"<<std::endl;
            return ;
        }
        totalsent+=bytes_sent;
    }
    return;
}

void util::sendres(int sock,int type,std::string &e){
    const char* s=e.c_str();
    sendres(sock,type,s);
}

std::string util:: utf8_to_gbk(const std::string &utf8_str) {
    iconv_t cd = iconv_open("GBK//TRANSLIT", "UTF-8");
    if (cd == (iconv_t)-1) {
        std::cerr << "iconv_open failed!" << std::endl;
        return "";
    }

    size_t in_bytes_left = utf8_str.size();
    size_t out_bytes_left = in_bytes_left * 2;  // GBK 编码最大可能是 UTF-8 的两倍
    char *in_buf = const_cast<char*>(utf8_str.c_str());
    char *out_buf = new char[out_bytes_left];
    char *out_ptr = out_buf;

    if (iconv(cd, &in_buf, &in_bytes_left, &out_ptr, &out_bytes_left) == (size_t)-1) {
        std::cerr << "iconv conversion failed!" << std::endl;
        iconv_close(cd);
        delete[] out_buf;
        return "";
    }

    std::string gbk_str(out_buf, out_ptr - out_buf);
    iconv_close(cd);
    delete[] out_buf;
    return gbk_str;
}

void util::sql_shutdown(int sock){
    std::string temp="server has some problems,please try later";
    sendres(sock,res_type::login_fail,temp);
}

