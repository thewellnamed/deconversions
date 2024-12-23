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

class EmotionsPlugin(AnalysisPlugin):
    def _load_synsets(self, synsets_path):
        tree = ET.parse(synsets_path)
        root = tree.getroot()
        pos_map = {"noun": self._wn.NOUN, "adj": self._wn.ADJ, "verb": self._wn.VERB, "adv": self._wn.ADV }

        synsets = {}
        for pos in ["noun", "adj", "verb", "adv"]:
            tag = pos_map[pos]
            synsets[tag] = {}
            for elem in root.findall(".//{0}-syn-list//{0}-syn".format(pos, pos)):
                offset = int(elem.get("id")[2:])
                if not offset: continue
                if elem.get("categ"):
                    categ = elem.get("categ")
                    emotion = Emo.emotions[categ] if categ in Emo.emotions else None
                    synsets[tag][offset] = set([emotion])
                elif elem.get("noun-id"):
                    noun_id = int(elem.get("noun-id")[2:])
                    emotions = synsets[pos_map["noun"]][noun_id]

                    if emotions:
                        if offset not in synsets[tag]:
                            synsets[tag][offset] = set()
                        for e in emotions:
                            synsets[tag][offset].add(e)
        return synsets

    def _load_emotions(self, hierarchy_path):
        """Loads the hierarchy of emotions from the WordNet-Affect xml."""
        tree = ET.parse(hierarchy_path)
        root = tree.getroot()
        for elem in root.findall("categ"):
            name = elem.get("name")
            if name == "root":
                Emo.emotions["root"] = Emo("root")
            else:
                Emo.emotions[name] = Emo(name, elem.get("isa"))

    def activate(self, *args, **kwargs):
        local_path = os.path.dirname(os.path.abspath(__file__)) + "/"
        self._wn_path = self.wn_path
        self._wn = WordNetCorpusReader(os.path.abspath("{0}".format(local_path + self._wn_path)), nltk.data.find(local_path + self._wn_path))

        self._load_emotions(local_path + self.hierarchy_path)
        self._synsets = self._load_synsets(local_path + self.synsets_path)

    def analyse_entry(self, entry, params):
        emotions = {}

        for pos in [self._wn.NOUN, self._wn.ADJ, self._wn.VERB, self._wn.ADV]:
            for offset in self._synsets[pos]:
                synset = self._wn.synset_from_pos_and_offset(pos, offset).name()

                for emotion in self._synsets[pos][offset]:
                    if emotion:
                        e = str(emotion)
                        if e in emotions:
                            emotions[e].add(synset)
                        else:
                            emotions[e] = set([synset])

        response = { "categories": emotions }
        yield response