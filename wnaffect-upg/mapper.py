import nltk
import os
import xml.etree.ElementTree as ET
from nltk.corpus import WordNetCorpusReader
from nltk.corpus.reader import WordNetError

class WordNetAffectMapper:
    
    def __init__(self, old_version_path, new_version_path, input_path, output_path):
        self.synsets_file = input_path
        self.output_file = output_path

        local_path = os.path.dirname(os.path.abspath(__file__)) + "/"
        old_version = os.path.abspath("{0}".format(local_path + old_version_path))
        new_version = os.path.abspath("{0}".format(local_path + new_version_path))

        # nltk.download(['wordnet', 'punkt', 'averaged_perceptron_tagger'])
        self.wno = WordNetCorpusReader(old_version, nltk.data.find(old_version))
        self.wnn = WordNetCorpusReader(new_version, nltk.data.find(new_version))
        self.parts = ["noun", "adj", "verb", "adv"]
        self.pos_map = { 'noun' : self.wno.NOUN, 'adj': self.wno.ADJ, 'verb': self.wno.VERB, 'adv': self.wno.ADV }
        
        # map nltk.pos_tag() output to wordnet pos
        self.wordnet_pos_map = {
            'NN': self.wnn.NOUN,
            'NNP': self.wnn.NOUN,
            'NNPS': self.wnn.NOUN,
            'NNS': self.wnn.NOUN,
            'JJ': self.wnn.ADJ,
            'JJR': self.wnn.ADJ,
            'JJS': self.wnn.ADJ,
            'RB': self.wnn.ADV,
            'RBR': self.wnn.ADV,
            'RBS': self.wnn.ADV,
            'VB': self.wnn.VERB,
            'VBD': self.wnn.VERB,
            'VBG': self.wnn.VERB,
            'VBN': self.wnn.VERB,
            'VBP': self.wnn.VERB,
            'VBZ': self.wnn.VERB
        }
        
    def translate(self):
        self.tree = ET.parse(self.synsets_file)
        self.root = self.tree.getroot()
        
        used_offsets = { 'noun' : {}, 'adj': {}, 'verb': {}, 'adv': {} }
        conflicts = { 'noun' : {}, 'adj': {}, 'verb': {}, 'adv': {} }
        out_map  = { 'noun' : {}, 'adj': {}, 'verb': {}, 'adv': {} }
    
        for pos in self.parts:
            for elem in self.root.findall(".//{0}-syn-list//{0}-syn".format(pos, pos)):
                offset = int(elem.get("id")[2:])
                if not offset: continue
                matched = False
    
                so = self.wno.synset_from_pos_and_offset(self.pos_map[pos], offset)            
                lemma, spos, sense = so.name().rsplit('.', 2)
                
                try:
                    sn = self.wnn.synset(so.name())
                    if sn.definition() == so.definition():
                        matched = True
                    else:
                        snarr = self.wnn.synsets(lemma, so.pos())
                    
                except WordNetError:
                    snarr = self.wnn.synsets(lemma, so.pos())
                
                if not matched:
                    if not snarr:
                        continue
                    
                    sn = self.find_best_match(so.definition(), snarr)
                            
                if sn.offset() in used_offsets[pos] and so.offset() not in used_offsets[pos][sn.offset()]:
                    if sn.offset() in conflicts[pos]:
                        conflicts[pos][sn.offset()].add(so.offset())
                    else:
                        conflicts[pos][sn.offset()] = set()
                        conflicts[pos][sn.offset()].add(so.offset())
                        for o in used_offsets[pos][sn.offset()]:
                            conflicts[pos][sn.offset()].add(o)
                
                if sn.offset() not in used_offsets[pos]:
                    used_offsets[pos][sn.offset()] = set()
                        
                used_offsets[pos][sn.offset()].add(so.offset())
                out_map[pos][so.offset()] = { "offset": sn.offset(), "label": sn.name() }
                
        self.offset_map = out_map
        self.conflicts = conflicts
        
        # resolve conflicts where multiple old synsets are mapped to the same new synset
        self.resolve_conflicts()
        
        # debugging
        exact = 0
        inexact = 0
        for pos in self.offset_map:
            for offset in self.offset_map[pos]:
                so = self.wno.synset_from_pos_and_offset(self.pos_map[pos], offset)
                sn = self.wnn.synset_from_pos_and_offset(self.pos_map[pos], self.offset_map[pos][offset]["offset"])
                if so.definition() == sn.definition():
                    exact += 1
                else:
                    print so.name() + " => " + sn.name()
                    print so.definition() + "\n" + sn.definition() + "\n"
                    inexact += 1
        print "exact matches: " + str(exact)
        print "inexact: " + str(inexact)
    
    def resolve_conflicts(self):
        for pos in self.parts:
            for offset in self.conflicts[pos]:
                sn = self.wnn.synset_from_pos_and_offset(self.pos_map[pos], offset)
                lemma, spos, sense = sn.name().rsplit('.', 2)
                senses = self.wnn.synsets(lemma, self.pos_map[pos])
                self.process_conflict(sn, pos, senses, self.conflicts[pos][offset]) 

 
    def process_conflict(self, sn, pos, senses, so_offsets):
        #print "conflict: " + sn.name() + " <= " + str(so_offsets)
        
        # first, find the best 1.6 match for the 3.1 synset
        so_synsets = []
        for next in so_offsets:
            so_synsets.append(self.wno.synset_from_pos_and_offset(self.pos_map[pos], next))
            
        match = self.find_best_match(sn.definition(), so_synsets)       
        self.offset_map[pos][match.offset()] = { "offset": sn.offset(), "label": sn.name() }
        so_offsets.remove(match.offset())
        senses.remove(sn)
        #print "matched " + match.name() + " (" + match.definition() + ") to " + sn.name() + " (" + sn.definition() + ")"
     
        # find new matches for remainder, or remove
        for next in so_offsets:
            so = self.wno.synset_from_pos_and_offset(self.pos_map[pos], next)
            
            if len(senses) == 0:
                #print "no more new senses remain. Deleting " + so.name()
                del self.offset_map[pos][next]
                continue
            
            match = self.find_best_match(so.definition(), senses)
           
            self.offset_map[pos][next] = { "offset": match.offset(), "label": match.name() }
            senses.remove(match)
            #print "matched " + so.name() + " (" + so.definition() + ") to " + match.name() + " (" + match.definition() + ")"
        
        #print "\n"
        return match
    
    def find_best_match(self, definition, synsets):
        if len(synsets) == 1:
            return synsets[0]
        
        context = nltk.pos_tag(nltk.word_tokenize(definition))
        context = [(token[0], self.wordnet_pos_map[token[1]]) for token in context if token[1] in self.wordnet_pos_map]
        
        match = None
        score = -1
        for s in synsets:
            result = sum(max([self.wnn.wup_similarity(s, test) for test in self.wnn.synsets(j, p)]+[0]) \
                            for j,p in context)
            
            if result > score:
                match = s
                score = result
        
        return match
        
    def write_output(self):
        output = open(self.output_file, "w")
        output_pos_map = { 'noun' : 'n', 'adj': 'a', 'verb': 'v', 'adv': 'r' }
        first = { 'noun' : True, 'adj': True, 'verb': True, 'adv': True }
        i = 0
        
        output.write("<syn-list>\n")
        
        for pos in self.parts:
            for elem in self.root.findall(".//{0}-syn-list//{0}-syn".format(pos, pos)):
                offset = int(elem.get("id")[2:])
                
                # skip synsets that have been removed
                if (offset not in self.offset_map[pos]):
                    continue
                next = self.offset_map[pos][offset]
                
                if (first[pos]):
                    first[pos] = False
                    if (i > 0):
                        output.write("    </{0}-syn-list>\n".format(self.parts[i-1]))
                    output.write("    <{0}-syn-list>\n".format(pos))
            
                if (pos == "noun"):
                    output.write("        <noun-syn id=\"n#{0}\" label=\"{1}\" categ=\"{2}\"/>\n".format(next["offset"], next["label"], elem.get("categ")))
                else:
                    noun = int(elem.get("noun-id")[2:])
                    if noun not in self.offset_map['noun']:
                        continue
                    
                    noun_id = self.offset_map['noun'][noun]["offset"]
                    
                    output.write("        <{0}-syn id=\"{1}#{2}\" label=\"{3}\" noun-id=\"n#{4}\" />\n"
                             .format(pos, output_pos_map[pos], next["offset"], next["label"], noun_id))
            i += 1
            
        output.write("    </{0}-syn-list>\n".format(pos))
        output.write("</syn-list>\n")
        output.close()
    