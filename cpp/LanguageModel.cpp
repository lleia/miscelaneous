#include "LanguageModel.h"

#include <set>
#include <map>
#include <vector>
#include <string>
#include <fstream>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <sys/stat.h>

using std::set;
using std::map;
using std::vector;
using std::string;
using std::exit;
using std::max;
using std::fstream;

namespace LM
{

#define ERR(...) fprintf(stderr,__VA_ARGS__)
#define LOG(...) fprintf(stdout,__VA_ARGS__)

#ifdef DEBUG
#define DLOG(...) fprintf(stdout,__VA_ARGS__)
#else
#define DLOG(...)
#endif

enum NgramType {
	UNIGRAM = 0,
	BIGRAM,
	TRIGRAM
};

struct GramTerm {
	string term[3];
	NgramType type;
	uint64_t freq;
	double prob;
	double backoff;
};

typedef map<string,struct GramTerm> DataMap;
typedef DataMap::iterator DataMapIter;
typedef DataMap::const_iterator DataMapConstIter;

static void trim(char *line)
{
	while (*line && (*line != '\r') && (*line != '\n')) line++;
	*line = '\0';
}

static void split(const string &query,vector<string> &item_list,char sep=' ')
{
	item_list.clear();
	const char *p = query.c_str();
	const char *q = query.c_str();
	while (true) {
		while (*p && (*p != sep)) p++;
		if (p != q)
			item_list.push_back(string(q,p-q));

		while (*p && (*p == sep)) p++;
		if (*p == '\0') break;
		q = p;
	}
}

static inline void updateGramTermFreq(DataMap &dict,const string &key,
		const struct GramTerm &term)
{
	DataMapIter it = dict.find(key);
	if (it == dict.end()) {
		dict[key] = term;
	} else {
		it->second.freq += term.freq;
	}
}

static inline void updateMap(map<string,set<string> > &dict,const string &key,
		const string &val)
{
	map<string,set<string> >::iterator it = dict.find(key);
	if (it == dict.end()) {
		set<string> val_set;
		val_set.insert(val);
		dict[key] = val_set;
	} else {
		it -> second.insert(val);
	}
}

static inline uint64_t getFileSize(const char *file)
{
	struct stat st;
	stat(file,&st);
	return st.st_size;
}

static inline void saveOrDie(const char *data,uint32_t record_size,
		uint32_t record_count,FILE *fp,const char *mes)
{
	if (fwrite(data,record_size,record_count,fp) != record_count) {
		ERR(mes);
		fclose(fp);
		exit(EXIT_FAILURE);
	}
}

static inline void readOrDie(char *data,uint32_t record_size,
		uint32_t record_count,FILE *fp,const char *mes)
{
	if (fread(data,record_size,record_count,fp) != record_count) {
		ERR(mes);
		fclose(fp);
		exit(EXIT_FAILURE);
	}
}

template <typename T,typename S>
static inline const T * bisearch(const T *begin,const T *end, const S &s)
{
	while (true) {
		int32_t size = end - begin;
		if (size <= 0)
			return NULL;
		const T *mid = begin + size / 2;
		if ((*mid) < s) {
		   begin = mid + 1;	
		   continue;
		}
		if (s < (*mid)) {
			end = mid;
			continue;
		}
		return mid;
	}
}

static inline bool operator < (const struct Bigram &big,uint32_t key_id)
{
	return (big.id < key_id);
}

static inline bool operator < (uint32_t key_id,const struct Bigram &big)
{
	return (key_id < big.id);
}

static inline bool operator < (const struct Trigram &tri,uint32_t key_id)
{
	return (tri.id < key_id);
}

static inline bool operator < (uint32_t key_id,const struct Trigram &tri)
{
	return (key_id < tri.id);
}

const char LanguageModel::TERM_SEP = ' ';
const char LanguageModel::FIELD_SEP = '\t';
const char *LanguageModel::OOV = "\x01";
const double LanguageModel::BIGRAM_DELTA = 0.2;
const double LanguageModel::TRIGRAM_DELTA = 0.1;
const double LanguageModel::UNIGRAM_INTERPOLATION = 0.5;
const uint32_t LanguageModel::OOV_PRUNING_THRESHOLD = 1u;
const uint32_t LanguageModel::CORPUS_PRUNING_THRESHOLD = 0;
const uint32_t LanguageModel::TRIGRAM_PRUNING_THRESHOLD = 1u;
const uint32_t LanguageModel::OOV_ID = 1u;
const int32_t LanguageModel::BUFFER_SIZE = 4096;

double LanguageModel::_bigram_delta = LanguageModel::BIGRAM_DELTA;
double LanguageModel::_trigram_delta = LanguageModel::TRIGRAM_DELTA;
uint32_t LanguageModel::_oov_id = LanguageModel::OOV_ID;
uint32_t LanguageModel::_oov_pruning_threshold =
	LanguageModel::OOV_PRUNING_THRESHOLD;
uint32_t LanguageModel::_trigram_pruning_threshold =
	LanguageModel::TRIGRAM_PRUNING_THRESHOLD;
uint32_t LanguageModel::_corpus_pruning_threshold =
	LanguageModel::CORPUS_PRUNING_THRESHOLD;
double LanguageModel::_unigram_interpolation =
	LanguageModel::UNIGRAM_INTERPOLATION;

LanguageModel::LanguageModel()
{
	_uni_buf = NULL;
	_bi_buf = NULL;
	_tri_buf = NULL;
}

LanguageModel::~LanguageModel()
{
	if (_uni_buf != NULL) {
		delete[] _uni_buf;
	}
	if (_bi_buf != NULL) {
		delete[] _bi_buf;
	}
	if (_tri_buf != NULL) {
		delete[] _tri_buf;
	}
}

inline void LanguageModel::release()
{
	if (_uni_buf != NULL) {
		delete[] _uni_buf;
	}
	if (_bi_buf != NULL) {
		delete[] _bi_buf;
	}
	if (_tri_buf != NULL) {
		delete[] _tri_buf;
	}
}

int LanguageModel::train(
		const char *data_file,
		const char *voca_file,
		const char *model_file,
		uint32_t oov_thresh,
		uint32_t corpus_thresh,
		uint32_t trigram_thesh)
{
	_oov_pruning_threshold = oov_thresh;
	_corpus_pruning_threshold = corpus_thresh;
	_trigram_pruning_threshold = trigram_thesh;

	LOG("Training Parameters:\n");
	LOG("oov pruning threshold : %u\n",_oov_pruning_threshold);
	LOG("corpus pruning threshold : %u\n",_corpus_pruning_threshold);
	LOG("trigram pruning threshold : %u\n",_trigram_pruning_threshold);

	FILE *fp_corpus = fopen(data_file,"r");
	if (fp_corpus == NULL) {
		ERR("Failed to open file %s\n",data_file);
		return 1;
	}

	char buffer[BUFFER_SIZE];
	DataMap gram_map;
	LOG("Start to load data...\n");
	LOG("First to generate the oov set...\n");
	while (fgets(buffer,BUFFER_SIZE,fp_corpus) != NULL) {
		/* query \t frequency */
		trim(buffer);
		vector<string> item_list;
		split(buffer,item_list,FIELD_SEP);
		if (item_list.size() != 2) {
			ERR("Illegal line :%s\n",buffer);
			continue;
		}
		string query = item_list[0];
		uint64_t freq = static_cast<uint64_t>(atoi(item_list[1].c_str()));
		DLOG("query = %s, freq = %llu\n",query.c_str(),freq);
		/* corpus pruning */
		if (freq <= _corpus_pruning_threshold)
			continue;
		freq -= _corpus_pruning_threshold;

		/* term切分结果 */
		split(query,item_list,TERM_SEP);

		for (size_t i = 0; i < item_list.size(); ++i) {
			string key = item_list.at(i);
			struct GramTerm uni = {{key,"",""},UNIGRAM,freq,0.0,0.0};
			updateGramTermFreq(gram_map,key,uni);
		}
	}
	fclose(fp_corpus);

	LOG("Start to process oov...\n");
	/* 词表截断并处理oov */
	set<string> oov_set;
	for (DataMapConstIter it = gram_map.begin();
			it != gram_map.end(); ++it) {
		if ((it->second.freq <= _oov_pruning_threshold) &&
				(it->second.type == UNIGRAM))
			oov_set.insert(it->first);
	}
	gram_map.clear();
	for (set<string>::iterator it = oov_set.begin();
			it != oov_set.end(); ++it) {
		DLOG("oov <%s>\n",it->c_str());
	}

	LOG("Start to load all grams...\n");
	fp_corpus = fopen(data_file,"r");
	/* Load all grams */
	DataMap modified_gram_map;
	while (fgets(buffer,BUFFER_SIZE,fp_corpus) != NULL) {
		/* query \t frequency */
		trim(buffer);
		vector<string> item_list;
		split(buffer,item_list,FIELD_SEP);
		if (item_list.size() != 2) {
			ERR("Illegal line :%s\n",buffer);
			continue;
		}
		string query = item_list[0];
		uint64_t freq = static_cast<uint64_t>(atoi(item_list[1].c_str()));
		DLOG("query = %s, freq = %llu\n",query.c_str(),freq);
		/* corpus pruning */
		if (freq <= _corpus_pruning_threshold)
			continue;
		freq -= _corpus_pruning_threshold;

		/* term切分结果 */
		split(query,item_list,TERM_SEP);

		for (size_t i = 0; i < item_list.size(); ++i)
			if (oov_set.find(item_list.at(i)) != oov_set.end())
				item_list[i] = OOV;

		for (size_t i = 0; i < item_list.size(); ++i) {
			const string &uni_term = item_list.at(i);
			string key = uni_term;
			struct GramTerm uni = {{uni_term,"",""},UNIGRAM,freq,0.0,0.0};
			updateGramTermFreq(modified_gram_map,key,uni);

			if (i + 1 < item_list.size()) {
				/* TERM_SEP是用以避免潜在的ab c和a bc的情况 */
				key.append(1,TERM_SEP);
				const string &bi_term = item_list.at(i+1);
				key.append(bi_term);
				struct GramTerm big = {{uni_term,bi_term,""},
					BIGRAM,freq,0.0,0.0};
				updateGramTermFreq(modified_gram_map,key,big);

				if (i + 2 < item_list.size()) {
					key.append(1,TERM_SEP);
					const string &tri_term = item_list.at(i+2);
					key.append(tri_term);
					struct GramTerm tri = {{uni_term,bi_term,tri_term},
						TRIGRAM,freq,0.0,0.0};
					updateGramTermFreq(modified_gram_map,key,tri);
				}
			}
		}
	}
	fclose(fp_corpus);

	LOG("Start to calculate the backoff paramters...\n");
	/* 频次为1和2的bigram/trigram个数，用于估计discounting参数 */
	uint64_t bigram_count_info[2] = {0u,0u};
	uint64_t trigram_count_info[3] = {0u,0u};
	/* for N(*w) */
	map<string,set<string> > modified_suffix_bigram_set;
	/* for N(w*) */
	map<string,set<string> > modified_prefix_bigram_set;
	/* for N(ww*) */
	map<string,set<string> > modified_prefix_trigram_set;
	/* frist loop,calculate N(*w)/N(w*)/N(ww*) */
	uint32_t uni_freq = 0;
	for (DataMapConstIter it = modified_gram_map.begin();
			it != modified_gram_map.end();++it) {
		const struct GramTerm &term = it->second;

		switch (term.type) {
		case BIGRAM:
			bigram_count_info[0] += (term.freq == 1) ? 1 : 0;
			bigram_count_info[1] += (term.freq == 2) ? 1 : 0;
			break;
		case TRIGRAM:
			trigram_count_info[0] += (term.freq == 1) ? 1 : 0;
			trigram_count_info[1] += (term.freq == 2) ? 1 : 0;
			break;
		default:
			break;
		}

		/* trigram pruning */
		if ((_trigram_pruning_threshold > 0) && (term.type == TRIGRAM) &&
				(term.freq <= _trigram_pruning_threshold))
			continue;

		const string &first_term = term.term[0];
		const string &second_term = term.term[1];
		const string &third_term = term.term[2];
		string key = first_term;
		switch (term.type) {
		case UNIGRAM:
			uni_freq += term.freq;
			continue;
		case BIGRAM:
			updateMap(modified_prefix_bigram_set,first_term,second_term);
			updateMap(modified_suffix_bigram_set,second_term,first_term);
			break;
		case TRIGRAM:
			key.append(1,TERM_SEP);
			key.append(second_term);
			updateMap(modified_prefix_trigram_set,key,third_term);
			break;
		default:
			break;
		}
	}

	if ((bigram_count_info[0] > 0) && (bigram_count_info[1] > 0))
		_bigram_delta = static_cast<double>(bigram_count_info[0]) /
			(bigram_count_info[0] + 2 * bigram_count_info[1]);
	LOG("bigram_count_info = <%llu,%llu>,bigram delta = %f\n",
			bigram_count_info[0],bigram_count_info[1],_bigram_delta);

	if ((trigram_count_info[0] > 0) && (trigram_count_info[1] > 0))
		_trigram_delta = static_cast<double>(trigram_count_info[0]) /
			(trigram_count_info[0] + 2 * trigram_count_info[1]);
	LOG("trigram_count_info = <%llu,%llu>,trigram delta = %f\n",
			trigram_count_info[0],trigram_count_info[1],_trigram_delta);

	/* second loop,calculate N(**) */
	uint64_t modified_bigram_count = 0;
	for (map<string,set<string> >::const_iterator it = 
			modified_prefix_bigram_set.begin();
			it != modified_prefix_bigram_set.end();
			++it) {
		modified_bigram_count += it->second.size(); 
	}
	DLOG("modified_bigram_count = %llu\n",modified_bigram_count);

	LOG("Start to calculate probability...\n");
	LOG("Start to calculate the unigram information...\n");
	/* 计算unigram信息 */
	for (DataMapIter it = modified_gram_map.begin();
			it != modified_gram_map.end();++it) {
		struct GramTerm &term = it->second;
		if ((term.type == BIGRAM) || (term.type == TRIGRAM))
			continue;
	
		/* unigram的概率是两种基本概率的插值 */	
		/* 正常的ngram估计 */
		double uni_prob = static_cast<double>(term.freq)/uni_freq;

		/* kneser-ney形式的unigram概率 */
		uint32_t kn_freq = 0;
		map<string,set<string> >::const_iterator uni_it =
			modified_suffix_bigram_set.find(term.term[0]);
		if (uni_it != modified_suffix_bigram_set.end())
			kn_freq = uni_it->second.size();

		double kn_prob = static_cast<double>(kn_freq) / modified_bigram_count;
		term.prob = _unigram_interpolation * uni_prob +
			(1 - _unigram_interpolation) * kn_prob;

		DLOG("%s\t%s\t%f\t%f\t%f\n",it->first.c_str(),
				term.term[0].c_str(),uni_prob,kn_prob,term.prob);

		/* 用于计算bigram的插值项 */
		uni_it = modified_prefix_bigram_set.find(term.term[0]);
		kn_freq = 1;
		if (uni_it != modified_prefix_bigram_set.end())
			kn_freq = uni_it->second.size();
		term.backoff = static_cast<double>(_bigram_delta * kn_freq) / term.freq;

		DLOG("%s\t%s\t%u\t%f\n",it->first.c_str(),
				term.term[0].c_str(),kn_freq,term.backoff);
	}

	LOG("Start to calculate the bigram information...\n");
	/* 计算bigram信息 */
	for (DataMapIter it = modified_gram_map.begin();
			it != modified_gram_map.end();++it) {
		struct GramTerm &term = it->second;
		if ((term.type == UNIGRAM) || (term.type == TRIGRAM))
			continue;
		const string &first_term = term.term[0];
		const string &second_term = term.term[1];

		DLOG("%s\t%s %s\t",it->first.c_str(),first_term.c_str(),
				second_term.c_str());

		DataMapConstIter uni_it = modified_gram_map.find(second_term);
		if (uni_it == modified_gram_map.end()) {
			ERR("failed to get unigram probabilities for term "
					"<%s> while computing bigram <%s> <%s>\n",
					second_term.c_str(),
					first_term.c_str(),
					second_term.c_str());
			exit(EXIT_FAILURE);
		}
		double backoff_unigram = uni_it->second.prob;

		uni_it = modified_gram_map.find(first_term);
		if (uni_it == modified_gram_map.end()) {
			ERR("failed to get unigram probabilities for term "
					"<%s> while computing bigram <%s> <%s>\n",
					second_term.c_str(),
					first_term.c_str(),
					second_term.c_str());
			exit(EXIT_FAILURE);
		}
		/* backoff term */
		term.prob = backoff_unigram * uni_it->second.backoff;
		DLOG("%f\t%f\t%f\n",backoff_unigram,uni_it->second.backoff,
				term.prob);
		/* main term */
		term.prob += max(term.freq - _bigram_delta,0.0)/uni_it->second.freq;

		/* 计算插值信息 */
		string key = first_term;
		key.append(1,TERM_SEP);
		key.append(second_term);

		uint32_t mod_freq = 1;
		map<string,set<string> >::const_iterator mod_it =
			modified_prefix_trigram_set.find(key);
		if (mod_it != modified_prefix_trigram_set.end()) {
			mod_freq = mod_it->second.size();
		} else {
			ERR("failed to get bigram modified frequency for "
					"<%s> <%s>\n",first_term.c_str(),second_term.c_str());
		}
		term.backoff = static_cast<double>(_trigram_delta * mod_freq) /
			term.freq;
	}

	LOG("Start to calculate the trigram information...\n");
	/* 计算trigram信息 */
	for (DataMapIter it = modified_gram_map.begin();
			it != modified_gram_map.end();++it) {
		struct GramTerm &term = it->second;
		if ((term.type == UNIGRAM) || (term.type == BIGRAM))
			continue;
		if ((_trigram_pruning_threshold > 0) &&
				(term.freq <= _trigram_pruning_threshold))
			continue;

		const string &first_term = term.term[0];
		const string &second_term = term.term[1];
		const string &third_term = term.term[2];

		DLOG("calculating trigram <%s,%s,%s>\n",first_term.c_str(),
				second_term.c_str(),third_term.c_str());

		/* 首先得到回退概率 */
		string key = second_term;
		key.append(1,TERM_SEP);
		key.append(third_term);
		DataMapConstIter bi_it = modified_gram_map.find(key); 
		if (bi_it == modified_gram_map.end()) {
			ERR("failed to find bigram probability for <%s> <%s>\
					while calculating <%s> <%s> <%s>\n",
					second_term.c_str(),third_term.c_str(),
					first_term.c_str(),second_term.c_str(),third_term.c_str());
			exit(EXIT_FAILURE);
		}

		double backoff_bigram = bi_it->second.prob;
		DLOG("backoff bigram <%s,%f>\n",key.c_str(),backoff_bigram);

		/* 回退参数 */
		key = first_term;
		key.append(1,TERM_SEP);
		key.append(second_term);
		bi_it = modified_gram_map.find(key);
		if (bi_it == modified_gram_map.end()) {
			ERR("failed to find bigram probability for <%s> <%s>\
					while calculating <%s> <%s> <%s>\n",
					first_term.c_str(),second_term.c_str(),
					first_term.c_str(),second_term.c_str(),third_term.c_str());
			exit(EXIT_FAILURE);
		}

		DLOG("backoff coeff <%s,%f>\n",key.c_str(),
				bi_it->second.backoff);

		term.prob = max(term.freq - _trigram_delta,0.0)/bi_it->second.freq;
		term.prob += backoff_bigram * bi_it->second.backoff;

		DLOG("freq = %llu,bi_freq = %llu,prob = %f\n",
				term.freq,bi_it->second.freq,term.prob);
	}

	/* save the models */
	LOG("Start to save the model to file...\n");
	FILE *fp_voca = fopen(voca_file,"w"); 
	if (fp_voca == NULL) {
		ERR("failed to open file <%s>\n",voca_file);
		return 1; 
	}

	FILE *fp_model = fopen(model_file,"w");
	if (fp_model == NULL) {
		ERR("failed to open file <%s>\n",model_file);
		return 2;
	}

	for (DataMapConstIter it = modified_gram_map.begin();
			it != modified_gram_map.end();++it) {
		const struct GramTerm &term = it->second;
		if (term.type == UNIGRAM)
			fprintf(fp_voca,"%s\n",term.term[0].c_str());
		if ((term.type == TRIGRAM) && (_trigram_pruning_threshold > 0) &&
				(term.freq <= _trigram_pruning_threshold))
			continue;
		assert((term.prob > 0.0) && (term.prob < 1.0));
		switch (term.type) {
		case UNIGRAM:
			fprintf(fp_model,"%s\t%g\t%g\n",
					term.term[0].c_str(),term.prob,term.backoff);
			break;
		case BIGRAM:
			fprintf(fp_model,"%s %s\t%g\t%g\n",
					term.term[0].c_str(),term.term[1].c_str(),term.prob,
					term.backoff);
			break;
		case TRIGRAM:
			fprintf(fp_model,"%s %s %s\t%g\t%g\n",
					term.term[0].c_str(),term.term[1].c_str(),
					term.term[2].c_str(),term.prob,term.backoff);
			break;
		default:
			break;
		}
	}

	fclose(fp_model);
	fclose(fp_voca);
	return 0;
}

int LanguageModel::build(const char *model_file,const char *index_file)
{
	FILE *fp_model = fopen(model_file,"r");
	if (fp_model == NULL) {
		ERR("failed to open model file <%s>\n",model_file);
		return 1;
	}

	/* 加载所有的unigram，确定所有的term id,并检查overflow */
	uint32_t term_id = _oov_id;
	vector<DARecord> term_id_list;
	map<string,uint32_t> term_id_map;

	uint64_t total_bigram_count = 0;
	uint64_t total_trigram_count = 0;
	char buffer[BUFFER_SIZE];

	while (fgets(buffer,BUFFER_SIZE,fp_model) != NULL) {
		trim(buffer);
		vector<string> field_list;
		split(buffer,field_list,'\t');
		if (field_list.size() != 3) {
			ERR("Invalid line in model file:<%s>\n",buffer); 
			exit(EXIT_FAILURE);
		}
		const string &phrase = field_list[0];

		DARecord record;
		vector<string> term_list;
		split(phrase,term_list,' ');
		switch (term_list.size()) {
		case 1:
			record.key = phrase;
			record.value = term_id;
			term_id_list.push_back(record);
			term_id_map[phrase] = term_id; 
			DLOG("insert term <%s> with id <%u>\n",phrase.c_str(),term_id);
			++term_id;
			break;
		case 2:
			++total_bigram_count;
			break;
		case 3:
			++total_trigram_count;
			break;
		default:
			ERR("Invalid grams in model file:<%s>\n",buffer);
			exit(EXIT_FAILURE);
		}
	}
	DLOG("bigram count = %llu,trigram_count = %llu\n",total_bigram_count,
			total_trigram_count);
	if ((total_bigram_count < 0) || (total_bigram_count >= 1ll<<32) ||
			(total_trigram_count < 0) || (total_trigram_count >= 1ll<<32)) {
		ERR("corpus overflow with bigram_count = %lld,trigram_count = %lld\n",
				total_bigram_count,total_trigram_count);
		exit(EXIT_FAILURE);
	}

	/* 载入gram数据 */
	vector<struct Unigram> unigram_list;
	vector<struct Bigram> bigram_list;
	vector<struct Trigram> trigram_list;
	fseek(fp_model,0,SEEK_SET);
	while (fgets(buffer,BUFFER_SIZE,fp_model) != NULL) {
		trim(buffer);
		vector<string> field_list;
		split(buffer,field_list,'\t');
		const string &phrase = field_list[0];
		double prob = atof(field_list[1].c_str());
		double backoff = atof(field_list[2].c_str());

		vector<string> term_list;
		split(phrase,term_list,' ');
		if (term_list.size() == 1) {
			if (unigram_list.size() > 0)
				unigram_list[unigram_list.size() - 1].end = bigram_list.size();
			if (bigram_list.size() > 0)
				bigram_list[bigram_list.size() - 1].end = trigram_list.size();
			struct Unigram uni = {prob,backoff,bigram_list.size(),0u};
			unigram_list.push_back(uni);
		} else if (term_list.size() == 2) {
			const string &second_term = term_list[1];
			if (bigram_list.size() > 0)
				bigram_list[bigram_list.size() - 1].end = trigram_list.size();
			/* 查询term id */
			map<string,uint32_t>::const_iterator it = term_id_map.
				find(second_term);
			if (it == term_id_map.end()) {
				ERR("failed to get term id for <%s>\n",second_term.c_str());
				fclose(fp_model);
				return 1;
			}
			struct Bigram big = {it->second,prob,backoff,
				trigram_list.size(),0u};
			bigram_list.push_back(big);
		} else if (term_list.size() == 3) {
			const string &third_term = term_list[2];
			map<string,uint32_t>::const_iterator it = term_id_map.
				find(third_term);
			if (it == term_id_map.end()) {
				ERR("failed to get term id for <%s>\n",third_term.c_str());
				fclose(fp_model);
				return 2;
			}
			struct Trigram tri = {it->second,prob};
			trigram_list.push_back(tri);
		} else {
			ERR("Invalid grams in model file:<%s>\n",buffer);
			fclose(fp_model);
			exit(EXIT_FAILURE);
		}
	}

	if (bigram_list.size() > 0)
		bigram_list[bigram_list.size() - 1].end = trigram_list.size();
	if (unigram_list.size() > 0)
		unigram_list[unigram_list.size() - 1].end = bigram_list.size();
	fclose(fp_model);

	/* build term da */
	DABuilder builder;
	builder.build(&term_id_list[0],&term_id_list[0] + term_id_list.size());

	/* 将模型写入索引文件 */
	FILE *fp_index = fopen(index_file,"wb");
	if (fp_index == NULL) {
		ERR("failed to open index file <%s>\n",index_file);
		fclose(fp_index);
		exit(EXIT_FAILURE);
	}

	uint64_t file_size = 0ull;
	uint64_t offset = 0ull;
	saveOrDie((char*)&file_size,1,sizeof(file_size),fp_index,"failed to save"
			"file size\n");
	offset += sizeof(file_size);

	uint32_t record_count = term_id_list.size();
	saveOrDie((char*)&record_count,1,sizeof(record_count),fp_index,"failed to"
			"save da records size!\n");
	offset += sizeof(record_count);

	uint32_t da_size = 0u;
	saveOrDie((char*)&da_size,1,sizeof(da_size),fp_index,"failed to save double"
			"array size!\n");
	offset += sizeof(da_size);

	/* datrie似乎只有fstream的接口,所以先关闭fp_index */
	fclose(fp_index);
	fstream out_index(index_file,std::ios::binary|std::ios::out|std::ios::app);
	out_index.seekp(offset);
	builder.write(out_index);
	out_index.close();

	/* 写入da的大小 */
	da_size = getFileSize(index_file) - offset;
	fp_index = fopen(index_file,"r+b");
	fseek(fp_index,sizeof(file_size) + sizeof(record_count),SEEK_SET);
	saveOrDie((char*)&da_size,1,sizeof(da_size),fp_index,"failed to save real"
			"double array size!\n");
	offset += da_size;

	/* store unigram data */
	fseek(fp_index,0,SEEK_END);
	uint32_t unigram_count = unigram_list.size();
	saveOrDie((char*)&unigram_count,1,sizeof(unigram_count),fp_index,
			"failed to save unigram count.\n");
	offset += sizeof(unigram_count);

	saveOrDie((char*)&unigram_list[0],sizeof(struct Unigram),unigram_count,
			fp_index,"failed to store unigram buffer.\n");
	offset += unigram_count * sizeof(struct Unigram);

	/* store bigram data */
	uint32_t bigram_count = bigram_list.size();
	saveOrDie((char*)&bigram_count,1,sizeof(bigram_count),fp_index,
			"failed to save bigram count.\n");
	offset += sizeof(bigram_count);

	saveOrDie((char*)&bigram_list[0],sizeof(struct Bigram),bigram_count,
			fp_index,"failed to store bigram buffer.\n");
	offset += bigram_count * sizeof(struct Bigram);

	/* store trigram data */
	uint32_t trigram_count = trigram_list.size();
	saveOrDie((char*)&trigram_count,1,sizeof(trigram_count),fp_index,"failed "
			"to save trigram count.\n");
	offset += sizeof(trigram_count);

	saveOrDie((char*)&trigram_list[0],sizeof(struct Trigram),trigram_count,
			fp_index,"failed to save trigram buffer.\n");
	offset += trigram_count * sizeof(struct Trigram);

	fclose(fp_index);

	/* check file size */
	file_size = getFileSize(index_file);
	if (file_size != offset) {
		ERR("file size error while file_size = %llu and offset = %llu!\n",
				file_size,offset);
		exit(EXIT_FAILURE);
	}

	/* store the file size */
	fp_index = fopen(index_file,"r+b");
	saveOrDie((char*)&file_size,1,sizeof(file_size),fp_index,"failed to store"
			" the real index size!\n");
	fclose(fp_index);
	return 0;
}

int LanguageModel::load(const char *index_file)
{
	uint64_t st_file_size = getFileSize(index_file);

	FILE *fp_index = fopen(index_file,"rb");
	if (fp_index == NULL) {
		ERR("failed to load language model index.\n");
		return 1;
	}

	/* check file size */
	uint64_t file_size = 0ull;
	uint64_t offset = 0ull;
	readOrDie((char*)&file_size,1,sizeof(file_size),fp_index,"read file size"
			" from index file error!\n");
	offset += sizeof(file_size);

	if (file_size != st_file_size) {
		ERR("the language model index has noncompatible file size,file size"
				"= %llu while %llu is expected.\n",st_file_size,file_size);
		return 2;
	}

	/* record count (term count in the DA) */
	uint32_t record_count;
	readOrDie((char*)&record_count,1,sizeof(record_count),fp_index,"failed to"
			" read record count from language model index.\n");
	offset += sizeof(record_count);

	/* da size */
	uint32_t da_size;
	readOrDie((char*)&da_size,1,sizeof(da_size),fp_index,"failed to load da"
			" size from the language model index.\n");
	offset += sizeof(da_size);

	/* load da */
	fclose(fp_index);
	fstream ifs(index_file,std::ios::in|std::ios::binary);
	ifs.seekp(offset);

	uint32_t read_size = _trie.read(ifs);
	DLOG("read_size = %u,expected %u\n",read_size,da_size);
	ifs.close();
	offset += da_size;

	if (record_count != _trie.size()) {
		ERR("noncompatible trie structure whose size is %u while %u"
				" expected.\n",_trie.size(),record_count);
		return 4;
	}

	/* reset the fp_index pointer */
	fp_index = fopen(index_file,"rb");
	fseek(fp_index,offset,SEEK_SET);

	/* unigram count */
	_uni_size = 0;
	readOrDie((char*)&_uni_size,1,sizeof(_uni_size),fp_index,"load unigram "
			"size from the language model index error!\n");
	/* load unigram buffer */
	DLOG("_uni_size = %u\n",_uni_size);
	_uni_buf = new struct Unigram [_uni_size];
	if (_uni_buf == NULL) {
		ERR("failed to create memory for _uni_buf.\n");
		release();
		return 5;
	}
	readOrDie((char*)_uni_buf,sizeof(struct Unigram),_uni_size,fp_index,
			"failed to load unigram buffer from index.\n");

	/* bigram count */
	_bi_size = 0;
	readOrDie((char*)&_bi_size,1,sizeof(_bi_size),fp_index,"load bigram "
			"size from the language model index error!\n");
	/* load bigram buffer */
	DLOG("_bi_size = %u\n",_bi_size);
	_bi_buf = new struct Bigram [_bi_size];
	if (_bi_buf == NULL) {
		ERR("failed to create memory for _bi_buf.\n");
		release();
		return 6;
	}
	readOrDie((char*)_bi_buf,sizeof(struct Bigram),_bi_size,fp_index,
			"failed to load bigram buffer from index.\n");

	/* trigram count */
	_tri_size = 0;
	readOrDie((char*)&_tri_size,1,sizeof(_tri_size),fp_index,"load trigram "
			"size from the language model index error!\n");
	/* load trigram buffer */
	DLOG("_tri_size = %u\n",_tri_size);
	_tri_buf = new struct Trigram [_tri_size];
	if (_tri_buf == NULL) {
		ERR("failed to create memory for _tri_buf.\n");
		release();
		return 7;
	}
	readOrDie((char*)_tri_buf,sizeof(struct Trigram),_tri_size,fp_index,
			"failed to load trigram buffer from index.\n");
	fclose(fp_index);

	/* check oov id */
	uint32_t term_id = 0u;
	if (!_trie.find(OOV,term_id) || (term_id != _oov_id)) {
		ERR("invalid oov id <%u> while <%u> expected.\n",term_id,_oov_id);
		release();
		return 8;
	}

	LOG("the language model has been loaded successfully!\n");
	return 0;
}

/* 得到一元概率值 */
double LanguageModel::getUnigramProb(const string &uni)
{
	uint32_t id = getTermID(uni);
	return getUnigramProb(id);
}

double LanguageModel::getUnigramProb(uint32_t id)
{
	return _uni_buf[id - 1].prob;
}

double LanguageModel::getLnUnigramProb(const string &uni)
{
	uint32_t id = getTermID(uni);
	return getLnUnigramProb(id);
}

double LanguageModel::getLnUnigramProb(uint32_t id)
{
	return log(_uni_buf[id - 1].prob);
}

/* 得到二元概率值 */
double LanguageModel::getBigramProb(const string &uni,const string &big)
{
	uint32_t uni_id = getTermID(uni);
	uint32_t bi_id = getTermID(big);
	return getBigramProb(uni_id,bi_id);
}

double LanguageModel::getBigramProb(uint32_t uni_id,uint32_t bi_id)
{
	const struct Unigram &st_uni = _uni_buf[uni_id - 1];
	/* search bigram term */
	const struct Bigram *st_bi = bisearch<struct Bigram,uint32_t>(
			&_bi_buf[0] + st_uni.begin,
			&_bi_buf[0] + st_uni.end,
			bi_id);
	double prob;
	if (st_bi == NULL) {
		double backoff_prob = getUnigramProb(bi_id);
		prob = backoff_prob * st_uni.backoff;
	} else {
		prob = st_bi->prob;
	}
	return prob;
}

double LanguageModel::getLnBigramProb(const string &uni,const string &big)
{
	uint32_t uni_id = getTermID(uni);
	uint32_t bi_id = getTermID(big);
	return getLnBigramProb(uni_id,bi_id);
}

double LanguageModel::getLnBigramProb(uint32_t uni_id,uint32_t bi_id)
{
	const struct Unigram &st_uni = _uni_buf[uni_id - 1];
	/* search bigram term */
	const struct Bigram *st_bi = bisearch<struct Bigram,uint32_t>(
			&_bi_buf[0] + st_uni.begin,
			&_bi_buf[0] + st_uni.end,
			bi_id);
	double prob;
	if (st_bi == NULL) {
		double backoff_prob = getUnigramProb(bi_id);
		prob = log(backoff_prob) + log(st_uni.backoff);
	} else {
		prob = log(st_bi->prob);
	}
	return prob;
}

/* 得到三元概率值 */ 
double LanguageModel::getTrigramProb(const string &uni,const string &big,
		const string &tri)
{
	uint32_t uni_id = getTermID(uni);
	uint32_t bi_id = getTermID(big);
	uint32_t tri_id = getTermID(tri);

	return getTrigramProb(uni_id,bi_id,tri_id);
}

double LanguageModel::getTrigramProb(uint32_t uni_id,uint32_t bi_id,
		uint32_t tri_id)
{
	const struct Unigram &st_uni = _uni_buf[uni_id - 1];
	const struct Bigram *ptr_st_bi = bisearch<struct Bigram,uint32_t>(
			&_bi_buf[0] + st_uni.begin,
			&_bi_buf[0] + st_uni.end,
			bi_id);

	if (ptr_st_bi != NULL) {
		const struct Trigram *ptr_st_tri = bisearch<struct Trigram,uint32_t>(
				&_tri_buf[0] + ptr_st_bi->begin,
				&_tri_buf[0] + ptr_st_bi->end,
				tri_id);
		if (ptr_st_tri != NULL)
			return ptr_st_tri->prob;
		double backoff_prob = getBigramProb(bi_id,tri_id);  
		return backoff_prob * ptr_st_bi->backoff;
	}

	return st_uni.prob * getUnigramProb(bi_id) * getUnigramProb(tri_id);
}

double LanguageModel::getLnTrigramProb(const string &uni,const string &big,
		const string &tri)
{
	uint32_t uni_id = getTermID(uni);
	uint32_t bi_id = getTermID(big);
	uint32_t tri_id = getTermID(tri);

	return getLnTrigramProb(uni_id,bi_id,tri_id);
}

double LanguageModel::getLnTrigramProb(uint32_t uni_id,uint32_t bi_id,
		uint32_t tri_id)
{
	const struct Unigram &st_uni = _uni_buf[uni_id - 1];
	const struct Bigram *ptr_st_bi = bisearch<struct Bigram,uint32_t>(
			&_bi_buf[0] + st_uni.begin,
			&_bi_buf[0] + st_uni.end,
			bi_id);

	if (ptr_st_bi != NULL) {
		const struct Trigram *ptr_st_tri = bisearch<struct Trigram,uint32_t>(
				&_tri_buf[0] + ptr_st_bi->begin,
				&_tri_buf[0] + ptr_st_bi->end,
				tri_id);
		if (ptr_st_tri != NULL)
			return log(ptr_st_tri->prob);
		double backoff_prob = getBigramProb(bi_id,tri_id);  
		return log(backoff_prob) + log(ptr_st_bi->backoff);
	}

	return log(st_uni.prob) + getLnUnigramProb(bi_id) +
		getLnUnigramProb(tri_id);
}

#undef ERR
#undef LOG
#undef DLOG
}
