# -*- coding: UTF-8 -*-
import sys

CN_UNICODE_INF = 0x4e00
CN_UNICODE_SUP = 0x9fcc

# 全角转半角
def d2s(str):
    ustr = unicode(str,'utf-8')
    ures = ''
    for uchar in ustr:
        inside_code = ord(uchar)
        if inside_code == 12288:
            inside_code = 32 
        elif inside_code >= 65281 and inside_code <= 65374:
            inside_code -= 65248
        else:
            pass
        ures += unichr(inside_code)
    return ures.encode('utf-8') 

# 半角转全角
def s2d(str):
    ustr = unicode(str,'utf-8')
    ures = ''
    for uchar in ustr:
        inside_code = ord(uchar)
        if inside_code == 32:
            inside_code = 12288 
        elif inside_code >= 32 and inside_code <= 126:
            inside_code += 65248
        else:
            pass
        ures += unichr(inside_code)
    return ures.encode('utf-8') 

def is_utf8_cn_char(str):
    code = map(lambda x:ord(x),str)
    if ((code[0] & 0xf0) != 0xe0) or \
            ((code[1] & 0xc0) != 0x80) or \
            ((code[2] & 0xc0) != 0x80):
        return FALSE 

    val = 0 
    val = (code[0] & 0x0f) << 12 | \
            ((code[1] >> 2) & 0x0f) << 8 |\
            (((code[1] & 0x03) << 2) | ((code[2] >> 4) & 0x03)) << 4 | \
            ((val << 4) | (code[2] & 0x0f))

    return (val >= CN_UNICODE_INF) and (val <= CN_UNICODE_SUP)

def good_byte(ch):
    return 0xc0 & ch == 0x80

def parse_utf8_seq(query):
    (id,count,code) = (0,len(query),map(lambda x:ord(x),query))
    term_list = []
    while id < count:
        # 1 byte
        if 0x80 & code[id] == 0x00:
            term_list.append(query[id])
            id += 1
            continue
        # 2 bytes
        if 0xe0 & code[id] == 0xc0:
            if (id + 1 < count) and good_byte(code[id + 1]):
                term_list.append(query[id:id + 2])
                id += 2
                continue
            else:
                return False,[]
        # 3 bytes 
        if 0xf0 & code[id] == 0xe0:
            if (id + 2 < count) and good_byte(code[id + 1]) and\
                    good_byte(code[id + 2]):
                term_list.append(query[id:id + 3])
                id += 3
                continue
            else:
                return False,[]
        # 4 bytes
        if 0xf8 & code[id] == 0xf0:
            if (id + 3 < count) and good_byte(code[id + 1]) and\
                    good_byte(code[id + 2]) and good_byte(code[id + 3]):
                term_list.append(query[id:id + 4])
                id += 4
                continue
            else:
                return False,[]
        # 5 bytes
        if 0xfc & code[id] == 0xf8:
            if (id + 3 < count) and good_byte(code[id + 1]) and\
                    good_byte(code[id + 2]) and good_byte(code[id + 3]) and\
                    good_byte(code[id + 4]):
                term_list.append(query[id:id + 5])
                id += 5
                continue
            else:
                return False,[]
        # 6 bytes
        if 0xfe & code[id] == 0xfc:
            if (id + 3 < count) and good_byte(code[id + 1]) and\
                    good_byte(code[id + 2]) and good_byte(code[id + 3]) and\
                    good_byte(code[id + 4]) and good_byte(code[id + 5]):
                term_list.append(query[id:id + 6])
                id += 6
                continue
            else:
                return False,[]
        return False,[]
    return True,term_list

'''
    get alignment map
'''
def alignment_map(first_string,second_string):
    len_a = len(first_string)
    len_b = len(second_string)

    dp = [([0]*(len_b+1))for i in range(len_a+1)]
    for i in range(0,len_a+1,1):
        dp[i][0] = i
    for j in range(0,len_b+1,1):
        dp[0][j] = j
    for i in range(1,len_a+1,1):
        for j in range(1,len_b+1,1):
            min = float("inf")
            if first_string[i-1] == second_string[j-1]:
                min = dp[i-1][j-1]
            else:
                min = dp[i-1][j-1]+1
            if min > dp[i-1][j]+1:
                min = dp[i-1][j]+1
            if min > dp[i][j-1]+1:
                min = dp [i][j-1]+1
            dp[i][j] = min

    # recovery the alignment
    align_map = list()
    i,j = len_a,len_b
    while i > 0 or j > 0:
        min = dp[i][j]
        if i >= 1 and j >= 1:
            if first_string[i-1] == second_string[j-1]:
                align_map.append((first_string[i-1],second_string[j-1]))
                i -= 1
                j -= 1
                continue
            else:
                if min == dp[i-1][j-1] + 1: 
                    align_map.append((first_string[i-1],second_string[j-1]))
                    i -= 1
                    j -= 1
                    continue
                if min == dp[i-1][j] + 1:
                    align_map.append((first_string[i-1],''))
                    i -= 1
                    continue
                if min == dp[i][j-1] + 1:
                    align_map.append(('',second_string[j-1]))
                    j -= 1
                    continue
        if i >= 1:
            align_map.append((first_string[i-1],''))
            i -= 1
            continue
        if j >= 1:
            align_map.append(('',second_string[j-1]))
            j -= 1
            continue
    align_map.reverse()
    return (dp[len_a][len_b],align_map)

'''
    BORROWED CODE, NEED TO MODITY
    edit distance of two strings

'''
def edit_distance(first_string,second_string):
    len_a = len(first_string)
    len_b = len(second_string)

    dp = [([0]*(len_b+1))for i in range(len_a+1)]
    for i in range(0,len_a+1,1):
        dp[i][0] = i
    for j in range(0,len_b+1,1):
        dp[0][j] = j
    for i in range(1,len_a+1,1):
        for j in range(1,len_b+1,1):
            min = float("inf")
            if first_string[i-1] == second_string[j-1]:
                min = dp[i-1][j-1]
            else:
                min = dp[i-1][j-1]+1
            if min > dp[i-1][j]+1:
                min = dp[i-1][j]+1
            if min > dp[i][j-1]+1:
                min = dp [i][j-1]+1
            dp[i][j] = min
    return dp[len_a][len_b]

'''
    BORROWED CODE, NEED TO MODIFY
    lcs of two strings
'''
def lcs(first_string,second_string):
    len1 = len(first_string)
    len2 = len(second_string)
    if len1 ==0:
        return len2
    elif len2==0:
        return len1
    dp = [([0]*(len2+1))for i in range(len1+1)]
    for i in range(0,len1,1):
        for j in range(0,len2,1):
            if first_string[i] == second_string[j]:
                dp[i+1][j+1] = dp[i][j]+1
            else:
                dp[i+1][j+1] = max(dp[i][j+1],dp[i+1][j])
    return dp[i+1][j+1]    

def cn_alignment_map(first_string,second_string):
    if first_string == None or second_string == None:
        return (-1,None)
    tag,first_list = parse_utf8_seq(first_string)
    if tag == False:
        print >> sys.stderr,'parse first string <%s> error!'%(first_string)
        return (-1,None)
    tag,second_list = parse_utf8_seq(second_string)
    if tag == False:
        print >> sys.stderr,'parse second string <%s> error!'%(second_string)
        return (-1,None)
    return alignment_map(first_list,second_list)

'''
    edit distance of two strings, the chinese 
'''
def cn_edit_distance(first_string,second_string):
    if first_string == None or second_string == None:
        return -1
    tag,first_list = parse_utf8_seq(first_string)
    if tag == False:
        return -1
    tag,second_list = parse_utf8_seq(second_string)
    if tag == False:
        return -1
    return edit_distance(first_list,second_list) 

'''
    lcs for chinese sequence
'''
def cn_lcs(first_string,second_string):
    if first_string == None or second_string == None:
        return -1
    tag,first_list = parse_utf8_seq(first_string)
    if tag == False:
        return -1
    tag,second_list = parse_utf8_seq(second_string)
    if tag == False:
        return -1
    return lcs(first_list,second_list)

'''
    parse pattern code for input string 
    Example:
        pattern_list = ['0','1','2','3']
        str = '2015 12 16'
        pattern = '1320'
'''
def parse_pattern_code(pattern_list,str):
    pos_dict = dict()
    fre_list = list()

    index = 0
    for item in pattern_list:
        fre_list.append(0)
        pos_dict[item] = index
        index += 1

    for item in str:
        index = pos_dict.get(item,-1)
        if index == -1:
            continue
        fre_list[index] += 1

    return ''.join(map(lambda x:'%d'%(x),fre_list))


if __name__ == '__main__':
    a = '高德公司'
    b = '阿里巴巴高德软件有限公司'
    (ed,edmap) = cn_alignment_map(a,b)
    print ed
    for item in edmap:
        print "%s,%s"%(item[0],item[1])

    a = '~!@#$%^&*()_+QWERTYUIOP{}ASDFGHJKL:"ZXCVBNM<>'
    print s2d(a)
    print d2s(s2d(a))
