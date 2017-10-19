import os
import math
import struct

def parse_keyword(fp):
    keyword = ''
    while True:
        c = fp.read(1)
        if c == ' ':
            break
        keyword += c
    return keyword

def parse_feature(fp,feature_size):
    feature = []
    for id in range(feature_size):
        feature.append(struct.unpack('f',fp.read(4))[0])
    #\n
    fp.read(1)
    sum = reduce(lambda x,y: x + y,map(lambda x:(x*x),feature))
    weight = math.sqrt(sum)
    return map(lambda x: x/weight,feature)

def parse_word_count(fp):
    count = ''
    while True:
        c = fp.read(1)
        if c == ' ':
            break
        count += c
    return int(count)

def parse_feature_size(fp):
    size = ''
    while True:
        c = fp.read(1)
        if c == '\n':
            break
        size += c
    return int(size)

def load_model(file):
    word_count = 0
    feature_size = 0
    feature_map = {}

    fp = open(file,'rb')
    word_count = parse_word_count(fp)
    feature_size = parse_feature_size(fp)
    for id in range(word_count):
        keyword = parse_keyword(fp)
        feature_map[keyword] = parse_feature(fp,feature_size)
    fp.close()
    return (word_count,feature_size,feature_map)
