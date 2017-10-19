#ifndef __DASTRIE_MAP_H__
#define __DASTRIE_MAP_H__

/*
 * Dastrie的简单封装，方便实际作数据索引,Dastrie的实际用法通常是string
 * 到整型的一个映射，这里增加了一个value数组，这样就能够方便地支持string
 * 到任意固定长度类型的数据映射，二进制索引文件的加载和读取逻辑也做了相应
 * 封装。
 * Li Lei
 * 2016-03-16
 * lilei.lilei@alibaba-inc.com
 */

#include <stdio.h>
#include <stdint.h>

#include <vector>
#include <string>
#include <fstream>
#include <exception>

#include <sys/stat.h>

#include "dastrie.h"

#define DAMAP_INT_TO_STR(value) #value
#define DAMAP_LINE_TO_STR(line) DAMAP_INT_TO_STR(line)
#define DAMAP_LINE_STR DAMAP_LINE_TO_STR(__LINE__)
#define DAMAP_THROW(msg) throw dasmap_excetion( \
  __FILE__ ":" DAMAP_LINE_STR ": exception: " msg)
#define DAMAP_ERROR(...) fprintf(stderr,__VA_ARGS__)

namespace dastrie {

using std::vector;
using std::string;
using std::fstream;
using std::exception;

static inline uint64_t getFileSize(const char *file)
{
	struct stat st;
	stat(file,&st);
	return st.st_size;
}

class dasmap_excetion : public std::exception {
    public:
        explicit dasmap_excetion(const char *msg = NULL)
            throw() : msg_(msg) {}
        dasmap_excetion(const dasmap_excetion &rhs)
            throw() : msg_(rhs.msg_) {}
        virtual ~dasmap_excetion() throw() {}
        virtual const char *what() const throw()
        {
            return (msg_ != NULL) ? msg_ : "";
        }

    private:
        const char *msg_;
        dasmap_excetion &operator=(const dasmap_excetion &);
};

template <typename T>
class dasmap {
    public:
        typedef T value_type;
        typedef uint32_t scope_type; 
        typedef dastrie::builder<string, scope_type> builder_type;
        typedef dastrie::trie<scope_type> trie_type;
        typedef builder_type::record_type record_type;

    public:
        dasmap();
        dasmap(const string & index_path);
        ~dasmap();

        bool load(const string & index_path);
        static bool build(const vector<string> & key_list, 
                const vector<value_type> & value_list,
                const string & index_path);
        bool find(const string & key,value_type & value) const;

    private:
        trie_type da_;
        value_type *value_list_;
        scope_type size_; 
};

template <typename T>
dasmap<T>::dasmap():value_list_(NULL),size_(0) {}

template <typename T>
dasmap<T>::dasmap(const string & index_path) {
    size_ = 0;
    value_list_ = NULL;
    load(index_path);
}

template <typename T>
bool dasmap<T>::load(const string & index_path) {
    size_ = 0;
    value_list_ = NULL;

    struct stat st;
    if (stat(index_path.c_str(),&st) != 0) {
        DAMAP_ERROR("Get file info error!");
        return false;
    }

    FILE *file = fopen(index_path.c_str(),"rb");
    if (file == NULL) {
        DAMAP_ERROR("Open index file error!");
        return false;
    }

    uint64_t offset = 0;
    uint64_t index_size = 0;
    if (fread((char*)&index_size,1,sizeof(index_size),file) 
            != sizeof(index_size)) {
        DAMAP_ERROR("Load the index size error!");
        fclose(file);
        return false;
    }
    offset += sizeof(index_size);

    if (index_size != static_cast<uint64_t> (st.st_size)) {
        DAMAP_ERROR("Illegal index size!");
        fclose(file);
        return false;
    }

    uint32_t da_size = 0u;
    if (fread((char*)&da_size,1,sizeof(da_size),file) != sizeof(da_size)) {
        DAMAP_ERROR("Load the da size error!");
        fclose(file);
        return false;
    }
    offset += sizeof(da_size);

    fclose(file);

    /* load da */
    fstream ifs(index_path.c_str(),std::ios::in|std::ios::binary);
    ifs.seekp(offset);
    uint32_t read_size = da_.read(ifs);
    ifs.close();

    if (read_size != da_size) {
        DAMAP_ERROR("Failed to load da,loaded size = %u while %u "
                "is expected!\n",read_size,da_size);
        return false;
    }
    offset += da_size;

    file = fopen(index_path.c_str(),"rb");
    if (file == NULL) {
        DAMAP_ERROR("Open DA file error!\n");
        return false;
    }
    fseek(file,offset,SEEK_SET);

    if (fread((char*)&size_,1,sizeof(size_),file) != sizeof(size_)) {
        fclose(file);
        DAMAP_ERROR("Load the value list size error!");
        return false;
    }

    value_list_ = new value_type [size_];
    if (value_list_ == NULL) {
        fclose(file);
        DAMAP_ERROR("Create value list error!");
        return false;
    }

    if (fread((char*)&value_list_[0],sizeof(value_type),size_,file) != size_) {
        delete [] value_list_;
        fclose(file);
        DAMAP_ERROR("Load value list error!\n");
        return false;
    }

    fclose(file);
    return true;
}


template <typename T>
dasmap<T>::~dasmap() {
    if (value_list_ != NULL)
        delete value_list_;
    value_list_ = NULL;
    size_ = 0;
}

template <typename T>
bool dasmap<T>::build(const vector<string> & key_list, 
        const vector<value_type> & value_list, const string & index_path) {

    if (key_list.size() != value_list.size()) {
        DAMAP_ERROR("Illegal value size");
        return false;
    }

    scope_type size = value_list.size();
    if (size == 0) {
        DAMAP_ERROR("Empty value set");
        return false;
    }

    vector<record_type> record_list;
    for (size_t i = 0; i < key_list.size(); ++i) {
        record_type record;
        record.key = key_list[i];
        record.value = i;
        record_list.push_back(record);
    } 

    builder_type builder;
    builder.build(&record_list[0],&record_list[0] + record_list.size());

    FILE *file = fopen(index_path.c_str(),"wb");
    if (file == NULL) {
        DAMAP_ERROR("Failed to open file");
        return false;
    }

    uint64_t index_size = 0;
    if (fwrite((char*)&index_size,1,sizeof(index_size),file) != sizeof(index_size)) {
        DAMAP_ERROR("Write index size error!");
        fclose(file);
        return false;
    }

    uint32_t da_size = 0u;
    if (fwrite((char*)&da_size,1,sizeof(da_size),file) != sizeof(da_size)) {
        DAMAP_ERROR("Write the da size error");
        fclose(file);
        return file;
    }
    fclose(file);

    fstream ofs(index_path.c_str(),std::ios::binary|std::ios::out|std::ios::app);
    ofs.seekp(sizeof(da_size) + sizeof(index_size));
    builder.write(ofs);
    ofs.close();

    da_size = getFileSize(index_path.c_str()) - sizeof(index_size)
        - sizeof(da_size);

    file = fopen(index_path.c_str(),"r+b");
    fseek(file,sizeof(index_size),SEEK_SET);
    if (fwrite((char*)&da_size,1,sizeof(da_size),file) != sizeof(da_size)) {
        DAMAP_ERROR("Write the da size error");
        fclose(file);
        return file;
    }
    fseek(file,0,SEEK_END);
 
    if (fwrite((char*)&size,1,sizeof(size),file) != sizeof(size)) {
        DAMAP_ERROR("Write the value list size error!");
        fclose(file);
        return false;
    }        

    if (fwrite((char*)&value_list[0],sizeof(value_type),size,file) != size) {
        DAMAP_ERROR("Write value list error");
        fclose(file);
        return false;
    }
    fclose(file);

    index_size = getFileSize(index_path.c_str());

    file = fopen(index_path.c_str(),"r+b");
    fseek(file,0,SEEK_SET);
    if (fwrite((char*)&index_size,1,sizeof(index_size),file) != sizeof(index_size)) {
        DAMAP_ERROR("Write the index size finally error!");
        fclose(file);
        return false;
    }
    fclose(file);

    return true;
}

template <typename T>
bool dasmap<T>::find(const string &key, value_type &value) const
{
    scope_type offset;
    if (!da_.find(key.c_str(),offset))
        return false;
    if (offset > size_) {
        return false;
    }
    value = value_list_[offset];  
    return true;
}

}
#endif
