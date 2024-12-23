from __future__ import division

import re
import nltk
import logging
import os
import string
import xml.etree.ElementTree as ET
from nltk.corpus import stopwords
from nltk.corpus import WordNetCorpusReader
from nltk.stem import PorterStemmer
from emotion import Emotion as Emo
from senpy.plugins import AnalysisPlugin
from demographics import Demographics

class WordnetPlugin(AnalysisPlugin):
    def activate(self, *args, **kwargs):
        nltk.download(['stopwords'])
                
        local_path = os.path.dirname(os.path.abspath(__file__)) + "/"
        self._wn = WordNetCorpusReader(os.path.abspath("{0}".format(local_path + self.wn_path)), nltk.data.find(local_path + self.wn_path))
                        
        self._stopwords = stopwords.words('english')
        for stop in ["n't", "'m", "'ve", "'re", "'s"]:
            self._stopwords.append(stop)
        
        self._porter = PorterStemmer()
        
        self._punctuation = set(string.punctuation)
        for p in ['.', '?', '!', ';', ':', ',']:
            self._punctuation.remove(p)
        
        # parts of speech to record
        self._pos_to_process = [ 
            'NN', 'NNP', 'NNPS', 'NNS',                 # nouns
            'JJ', 'JJR', 'JJS',                         # adjectives
            'RB', 'RBR', 'RBS',                         # adverbs
            'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ',    # verbs
            'CD', 'FW', 'PRP$'                          # numbers, foreign words, possessive
        ]

        # map nltk.pos_tag() output to wordnet pos
        self._wordnet_pos_map = {
            'NN': self._wn.NOUN,
            'NNP': self._wn.NOUN,
            'NNPS': self._wn.NOUN,
            'NNS': self._wn.NOUN,
            'JJ': self._wn.ADJ,
            'JJR': self._wn.ADJ,
            'JJS': self._wn.ADJ,
            'RB': self._wn.ADV,
            'RBR': self._wn.ADV,
            'RBS': self._wn.ADV,
            'VB': self._wn.VERB,
            'VBD': self._wn.VERB,
            'VBG': self._wn.VERB,
            'VBN': self._wn.VERB,
            'VBP': self._wn.VERB,
            'VBZ': self._wn.VERB
        }
        
        self._lemma_cache = { 
            self._wn.NOUN: {}, 
            self._wn.ADJ: {}, 
            self._wn.ADV: {}, 
            self._wn.VERB: {} 
        }
        
        self._path_cache = {}
        self._demo = Demographics(self._wn);

    def analyse_entry(self, entry, params):
        text_input = entry.get("nif:isString", None)
        text = self._preprocess_text(text_input)
        
        self._cache_misses = 0
        output = self._process(text)
        
        print "cache misses: " + str(self._cache_misses)
        yield output

    def _preprocess_text(self, text):
        regHttp = re.compile(
            '(http://)[a-zA-Z0-9]*.[a-zA-Z0-9/]*(.[a-zA-Z0-9]*)?')
        regHttps = re.compile(
            '(https://)[a-zA-Z0-9]*.[a-zA-Z0-9/]*(.[a-zA-Z0-9]*)?')
        regAt = re.compile('@([a-zA-Z0-9]*[*_/&%#@$]*)*[a-zA-Z0-9]*')
        
        text = re.sub(regHttp, '', text)
        text = re.sub(regHttps, '', text)
        
        text = ''.join(ch if ch not in self._punctuation else ' ' for ch in text)
        return text

    def _process(self, text):
        sentences = nltk.pos_tag_sents([nltk.word_tokenize(sent) for sent in nltk.sent_tokenize(text)])
        output = { "parsed": [], "terms": {}, "demo": {} }

        for sentence in sentences:
            processed = self._process_sentence(sentence)

            i = 0

            for word,lemma,tag,stop,pos,offset in processed:
                if stop:
                    synset = None
                else:
                    synset = self._wn.synset_from_pos_and_offset(pos, offset) if offset else None
                    ss = synset.name() if synset else "none"
                    output["parsed"].append(ss if synset else lemma)

                    if lemma in output["terms"]:
                        if ss in output["terms"][lemma]:
                            output["terms"][lemma][ss] += 1
                        else:
                            output["terms"][lemma][ss] = 1
                    else:
                        output["terms"][lemma] = { ss: 1 }

                    self._demo.process((word, lemma, tag, pos, synset), sentence, processed, i, output)
                i += 1
                    
        return output
                
    def _process_sentence(self, sentence):
        output = []
        
        for token in sentence:
            word = token[0].lower()
            tag = token[1]

            if tag in self._pos_to_process:
                stop = word in self._stopwords
                if tag in self._wordnet_pos_map:
                    pos = self._wordnet_pos_map[tag]
                    lemma = self._wn.morphy(word, pos) or word
                else:
                    pos = None
                    lemma = self._porter.stem(word)
                    
                output.append([word, lemma, tag, stop, pos])
                
        return self._match_synsets(output)

    def _match_synsets(self, tokens):
        output = []     
        context = self._populate_synsets(tokens)
        context_len = len(context)

        i = 0
        for token in tokens:
            lemma = token[1]
            stop = token[3]
            pos = token[4]
            
            # if not a part of speech found in wordnet, ignore
            if stop or not pos:
                token.append(None)
                output.append(token)
                i += 1
                continue
            
            synsets = context[i]

            # if no synsets found, ignore
            if not synsets:
                token.append(None)
                output.append(token)
                i += 1
                continue
            
            # if only one possibility, use it
            if len(synsets) == 1:
                token[4] = synsets[0].pos()
                token.append(synsets[0].offset())
                output.append(token)
                i += 1
                continue
            
            # find best match using up to 3 context words on either side
            context_synsets = []
            next = i - 1
            pre = 0
            while pre < 3 and next >= 0:
                if context[next]:
                    pre += 1
                next -= 1
            
            found = 0
            while found < 6 and next >= 0 and next < context_len:
                if context[next]:
                    context_synsets.append(context[next])
                    found += 1
                next += 1
            
            match = self._find_best_synset(synsets, context_synsets) 
            token[4] = match.pos()
            token.append(match.offset())
            output.append(token)
            i += 1
        
        return output       
    
    def _populate_synsets(self, tokens):
        context = []

        for _,lemma,_,stop,pos in tokens:
            if pos and not stop:
                if lemma in self._lemma_cache[pos]:
                    context.append(self._lemma_cache[pos][lemma])
                else:
                    cs = self._wn.synsets(lemma, pos)
                    if not cs:
                        cs = self._wn.synsets(lemma)
                    self._lemma_cache[pos][lemma] = cs or None
                    context.append(cs or None)
            else:
                context.append(None)
        
        return context
    
    def _find_best_synset(self, synsets, context):
        score = -1
        for s in synsets:
            result = sum(max([self._path_similarity(s, test) for test in ss]+[0]) for ss in context)
            
            if result > score:
                match = s
                score = result
        
        return match
    
    def _path_similarity(self, s1, s2):
        s1o,s2o = sorted([s1.offset(), s2.offset])
        
        if s1o in self._path_cache and s2o in self._path_cache[s1o]:
            return self._path_cache[s1o][s2o]
        else:
            self._cache_misses += 1
            result = self._wn.wup_similarity(s1, s2)
            if s1o in self._path_cache:
                self._path_cache[s1o][s2o] = result
            else:
                self._path_cache[s1o] = { s2o: result }
