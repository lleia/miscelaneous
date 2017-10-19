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
		/* ѵ������Ƶ�� */
		static const uint32_t CORPUS_PRUNING_THRESHOLD;
		/* Ĭ�ϵ�oov�ض�Ƶ�� */
		static const uint32_t OOV_PRUNING_THRESHOLD;
		/* trigram pruning threshold */
		static const uint32_t TRIGRAM_PRUNING_THRESHOLD;

	private:
		/* Ĭ��term�ָ�� */
		static const char TERM_SEP;
		/* Ĭ��field�ָ�����ָ�query��frequency,ngram�͸��� */
		static const char FIELD_SEP;
		/* Ĭ��OOV��ʾ */
		static const char *OOV;
		/* 
		 * Ĭ�ϵ�discounting����,���Ȳ������ݹ���,�ڹ���ʧЧʱ������
		 * �����Ĭ�ϲ���.
		 */
		static const double BIGRAM_DELTA;
		static const double TRIGRAM_DELTA;
		/* Ĭ�ϵ�unigram��ֵ���� */ 
		static const double UNIGRAM_INTERPOLATION;
		/* OOV��term id */
		static const uint32_t OOV_ID;
		/* Ĭ�ϵ�string buffer��С */
		static const int32_t BUFFER_SIZE;

	private:
		/* Kneser Ney Smoothing��bigram��discounting���� */
		static double _bigram_delta;
		/* Kneser Ney Smoothing��trigram��discounting���� */
		static double _trigram_delta;
		/* �ʱ�ض�threshold */
		static uint32_t _oov_pruning_threshold;
		/* Ĭ�ϵ�oov id */
		static uint32_t _oov_id;
		/* corpus pruning size */
		static uint32_t _corpus_pruning_threshold;
		/* pruning size */
		static uint32_t _trigram_pruning_threshold;
		/* unigram�Ĳ�ֵϵ�� */
		static double _unigram_interpolation;

	private:
		/* �ʱ�ӳ�� */
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
		/* ѵ��ngram����ģ�� */
		static int train(
				const char *data_file,
				const char *vocal_file,
				const char *model_file,
				uint32_t oov_thresh = OOV_PRUNING_THRESHOLD,
				uint32_t corpus_thresh = CORPUS_PRUNING_THRESHOLD,
				uint32_t trigram_thesh = TRIGRAM_PRUNING_THRESHOLD);
		/* ����LM�����ļ� */
		static int build(const char *model_file,const char *index_file);
		/* �������� */
		int load(const char *index_file);
		/* �õ�term id */
		uint32_t getTermID(const string &term);
		/* �õ�һԪ����ֵ */
		double getUnigramProb(const string &uni); 
		double getUnigramProb(uint32_t id);
		double getLnUnigramProb(const string &uni); 
		double getLnUnigramProb(uint32_t id);
		/* �õ���Ԫ����ֵ */
		double getBigramProb(const string &uni,const string &big);
		double getBigramProb(uint32_t uni_id,uint32_t bi_id);
		double getLnBigramProb(const string &uni,const string &big);
		double getLnBigramProb(uint32_t uni_id,uint32_t bi_id);
		/* �õ���Ԫ����ֵ */ 
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
