#include <vector>
#include <string>

static void split(const std::string &query,
        std::vector<std::string> &item_list,char sep=' ')
{
    item_list.clear();
    const char *b = query.c_str();
    const char *p = query.c_str();
    const char *q = query.c_str();
    while (true) {
        while (*q && *q == sep) {
            if (q == b || *(q - 1) == sep)
                item_list.push_back("");
            q++;
        }
        if (*q == '\0') break;
        p = q;
        while (*p && *p != sep) {
            p++;
        }
        if (*p == '\0') break;
        item_list.push_back(std::string(q,p-q));
        q = p;
    }
    if (*q != '\0')
        item_list.push_back(std::string(q));
    else if (*(q - 1) == sep)
        item_list.push_back("");
    else
        ;
}

static int parse_cn_seq(const std::string &cn_seq,std::vector<string> &cn_list)
{
    cn_list.clear();

    for (int i = 0; cn_seq[i] != '\0'; ) {  
        char chr = cn_seq[i];  
        //chr是0xxx xxxx，即ascii码  
        if ((chr & 0x80) == 0) {  
            cn_list.push_back(cn_seq.substr(i,1));
            ++i;  
        } else if ((chr & 0xF8) == 0xF8) {  
            cn_list.push_back(cn_seq.substr(i,5));
            i+=5;  
        } else if ((chr & 0xF0) == 0xF0) {  
            cn_list.push_back(cn_seq.substr(i,4));
            i+=4;  
        } else if ((chr & 0xE0) == 0xE0) {  
            cn_list.push_back(cn_seq.substr(i,3));
            i+=3;  
        } else if ((chr & 0xC0) == 0xC0) { 
            cn_list.push_back(cn_seq.substr(i,2));
            i+=2;  
        } else {
            cn_list.clear();
           return 0;
        } 
    }
    return cn_list.size();
}

