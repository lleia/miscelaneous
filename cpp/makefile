CXX = g++
CXXFLAGS += -Wall -O2 -c

ALL:dastrie_sample dasmap_sample checking

LanguageModel.o:LanguageModel.cpp
	$(CXX) $(CXXFLAGS) LanguageModel.cpp

checking.o:checking.cpp
	$(CXX) $(CXXFLAGS) checking.cpp

dasmap_sample.o:dasmap_sample.cpp
	$(CXX) $(CXXFLAGS) dasmap_sample.cpp

dastrie_sample.o:dastrie_sample.cpp
	$(CXX) $(CXXFLAGS) dastrie_sample.cpp

dastrie_sample:dastrie_sample.o
	$(CXX) dastrie_sample.o -o dastrie_sample

checking:LanguageModel.o checking.o
	$(CXX) -pthread LanguageModel.o checking.o -o checking

dasmap_sample:dasmap_sample.o
	$(CXX) dasmap_sample.o -o dasmap_sample

clean:
	rm -f *.o dastrie_sample dasmap_sample checking
