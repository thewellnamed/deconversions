from __future__ import division

import json
import networkx as nx
import nltk
import os

from copy import deepcopy
from nltk.corpus import WordNetCorpusReader
from senpy.plugins import AnalysisPlugin

class ChainsPlugin(AnalysisPlugin):
    def activate(self, *args, **kwargs):
        local_path = os.path.dirname(os.path.abspath(__file__)) + "/"
        self._wn_path = self.wn_path
        self._wn = WordNetCorpusReader(os.path.abspath("{0}".format(local_path + self._wn_path)), nltk.data.find(local_path + self._wn_path))

        self._SCORE_SYNSET = 3
        self._SCORE_HYPERNYM = 2
        self._SCORE_MERONYM = 1
        self._SCORE_NONE = 0

    def analyse_entry(self, entry, params):
        input = json.loads(entry.get("nif:isString", None))
        response = {}
        response["chains"] = self._find_lexical_chains(input)

        yield response

    def _find_lexical_chains(self, input):
        chains = []
        graph = nx.Graph()
        lookup = {}

        i1 = deepcopy(input)
        i2 = deepcopy(input)
        i2.pop(0)

        iter = 0
        while i1:
            e1 = i1.pop(0)
            lookup[e1['name']] = e1['count']
            s1 = self._wn.synset(e1['name'])
            if i2:
                for e2 in i2:
                    lookup[e2['name']] = e2['count']
                    iter += 1
                    s2 = self._wn.synset(e2['name'])
                    score = self._get_relationship_score(s1, s2)

                    if score > 0:
                        edge_score = (e1['count'] * self._SCORE_SYNSET) + \
                                     (e2['count'] * self._SCORE_SYNSET) + \
                                     ((e1['count'] + e2['count']) * score)
                        graph.add_edge(e1['name'], e2['name'], weight=edge_score)
                i2.pop(0)

        print "graph complete (" + str(iter) + "). finding subgraphs..."

        for g in list(nx.connected_component_subgraphs(graph)):
            nodes = [ { "synset": n, "def": self._wn.synset(n).definition(), "count": lookup[n] } for n in list(g.nodes)]
            nodes.sort(key=lambda x: x['count'], reverse=True)
            chains.append({ "synsets" : nodes, "score": g.size('weight') })

        chains.sort(key=lambda c: c['score'], reverse=True)
        return chains

    def _get_relationship_score(self, s1, s2):
        if s2 in s1.hypernyms():
            return self._SCORE_HYPERNYM

        elif s2 in s1.instance_hypernyms():
            return self._SCORE_HYPERNYM

        if s2 in s1.hyponyms():
            return self._SCORE_HYPERNYM

        elif s2 in s1.instance_hyponyms():
            return self._SCORE_HYPERNYM

        elif s2 in s1.part_meronyms():
            return self._SCORE_MERONYM

        elif s2 in s1.substance_meronyms():
            return self._SCORE_MERONYM

        elif s2 in s1.member_meronyms():
            return self._SCORE_MERONYM

        elif s2 in s1.part_holonyms():
            return self._SCORE_MERONYM

        elif s2 in s1.substance_holonyms():
            return self._SCORE_MERONYM

        elif s2 in s1.member_holonyms():
            return self._SCORE_MERONYM

        return self._SCORE_NONE
