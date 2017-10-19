#ifndef __LANGUAGE_MODEL__
#define __LANGUAGE_MODEL__

#include <string>
#include <vector>

#include "dastrie.h"

using std::string;
using std::vector;

namespace LM
{

struct Unigram {
	double prob;
	double backoff;
	uint32_t begin;
	uint32_t end;
};

struct Bigram {
	uint32_t id;
	double prob;
	double backoff;
	uint32_t begin;
	uint32_t end;
};

struct Trigram {
	uint32_t id;
	double prob;
};

class LanguageModel {
	typedef dastrie::builder<string,uint32_t> DABuilder;
	typedef dastrie::trie<uint32_t> DATrie;
	typedef DABuilder::record_type DARecord;

	public:
		/* 训练数据频率 */
		static const uint32_t CORPUS_PRUNING_THRESHOLD;
		/* 默认的oov截断频次 */
		static const uint32_t OOV_PRUNING_THRESHOLD;
		/* trigram pruning threshold */
		static const uint32_t TRIGRAM_PRUNING_THRESHOLD;

	private:
		/* 默认term分割符 */
		static const char TERM_SEP;
		/* 默认field分割符，分割query和frequency,ngram和概率 */
		static const char FIELD_SEP;
		/* 默认OOV表示 */
		static const char *OOV;
		/* 
		 * 默认的discounting参数,优先采用数据估计,在估计失效时，采用
		 * 这里的默认参数.
		 */
		static const double BIGRAM_DELTA;
		static const double TRIGRAM_DELTA;
		/* 默认的unigram插值参数 */ 
		static const double UNIGRAM_INTERPOLATION;
		/* OOV的term id */
		static const uint32_t OOV_ID;
		/* 默认的string buffer大小 */
		static const int32_t BUFFER_SIZE;

	private:
		/* Kneser Ney Smoothing中bigram的discounting参数 */
		static double _bigram_delta;
		/* Kneser Ney Smoothing中trigram的discounting参数 */
		static double _trigram_delta;
		/* 词表截断threshold */
		static uint32_t _oov_pruning_threshold;
		/* 默认的oov id */
		static uint32_t _oov_id;
		/* corpus pruning size */
		static uint32_t _corpus_pruning_threshold;
		/* pruning size */
		static uint32_t _trigram_pruning_threshold;
		/* unigram的插值系数 */
		static double _unigram_interpolation;

	private:
		/* 词表映射 */
		DATrie _trie;

		/* Ngram buffers */
		uint32_t _uni_size;
		struct Unigram *_uni_buf;
		uint32_t _bi_size;
		struct Bigram *_bi_buf;
		uint32_t _tri_size;
		struct Trigram *_tri_buf;

	private:
		void release();
	public:
		/* 训练ngram概率模型 */
		static int train(
				const char *data_file,
				const char *vocal_file,
				const char *model_file,
				uint32_t oov_thresh = OOV_PRUNING_THRESHOLD,
				uint32_t corpus_thresh = CORPUS_PRUNING_THRESHOLD,
				uint32_t trigram_thesh = TRIGRAM_PRUNING_THRESHOLD);
		/* 创建LM索引文件 */
		static int build(const char *model_file,const char *index_file);
		/* 载入索引 */
		int load(const char *index_file);
		/* 得到term id */
		uint32_t getTermID(const string &term);
		/* 得到一元概率值 */
		double getUnigramProb(const string &uni); 
		double getUnigramProb(uint32_t id);
		double getLnUnigramProb(const string &uni); 
		double getLnUnigramProb(uint32_t id);
		/* 得到二元概率值 */
		double getBigramProb(const string &uni,const string &big);
		double getBigramProb(uint32_t uni_id,uint32_t bi_id);
		double getLnBigramProb(const string &uni,const string &big);
		double getLnBigramProb(uint32_t uni_id,uint32_t bi_id);
		/* 得到三元概率值 */ 
		double getTrigramProb(const string &uni,const string &big,
				const string &tri);
		double getTrigramProb(uint32_t uni_id,uint32_t bi_id,uint32_t tri_id);
		double getLnTrigramProb(const string &uni,const string &big,
				const string &tri);
		double getLnTrigramProb(uint32_t uni_id,uint32_t bi_id,uint32_t tri_id);
	public:
		LanguageModel();
		~LanguageModel();
};

inline uint32_t LanguageModel::getTermID(const string &term)
{
	uint32_t id = 0;
	/* oov id by default */
	if (!_trie.find(term.c_str(),id))
		id = _oov_id;
	return id;
}

}
#endif
