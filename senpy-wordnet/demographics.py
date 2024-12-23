class Demographics:
    # trigger type
    TRIGGER_WORD = 0
    TRIGGER_LEMMA = 1
    TRIGGER_TAG = 2
    TRIGGER_HYPERNYM = 3
    TRIGGER_PHRASE = 4
    TRIGGER_FUNC = 5

    # token lookup
    TOKEN_WORD = 0
    TOKEN_LEMMA = 1
    TOKEN_TAG = 2
    TOKEN_POS = 3
    TOKEN_SS = 4

    def __init__(self, wn):
        self._wn = wn

    def process(self, token, sentence, tokens, index, output):
        for demo in [ "age", "gender", "denom" ]:
            # check triggers
            for trigger in self.triggers[demo]:
                match,reverse = self.check_conditions(demo, trigger, token, tokens, index)
                if match:
                    val = { "val": self.get_value(trigger, token, reverse), "context": " ".join([ p[0] for p in sentence ]) }
                    if demo in output['demo']:
                        if val not in output['demo'][demo]:
                            output['demo'][demo].append(val)
                    else:
                        output['demo'][demo] = [ val ]

    def check_conditions(self, demo, trigger, token, tokens, index):
        have_trigger = False

        if trigger['type'] == self.TRIGGER_LEMMA:
            have_trigger = token[self.TOKEN_LEMMA] == trigger['val']
        elif trigger['type'] == self.TRIGGER_TAG:
            have_trigger = token[self.TOKEN_TAG] == trigger['val']

            if trigger['val'] == 'CD':
                try:
                    val = int(token[self.TOKEN_LEMMA])
                except ValueError:
                    have_trigger = False

        elif trigger['type'] == self.TRIGGER_HYPERNYM:
            if token[self.TOKEN_SS]:
                hyp = lambda s:s.hypernyms() + s.instance_hypernyms()
                hypernyms = [s.name() for s in list(token[self.TOKEN_SS].closure(hyp))]
                have_trigger = trigger['val'] in hypernyms
        elif trigger['type'] == self.TRIGGER_PHRASE:
            have_trigger = True
            for i in range(index, index + len(trigger['val'])):
                if len(tokens) < i+1 or tokens[i][self.TOKEN_LEMMA] != trigger['val'][i - index]:
                    have_trigger = False
                    break
        elif trigger['type'] == self.TRIGGER_FUNC:
            have_trigger = trigger['func'](trigger, token)

        if have_trigger:
            # check context
            context = [ c for c in self.context[demo] if 'triggers' in c and trigger['id'] in c['triggers'] ]
            reverse = [ c for c in self.context[demo] if 'reverse' in c and trigger['id'] in c['reverse'] ]

            if len(context) == 0 and len(reverse) == 0:
                return True, False

            have_context = False
            have_reverse = False

            if len(reverse) > 0:
                for c in reverse:
                    part = self.TOKEN_WORD if c['type'] == self.TRIGGER_WORD else self.TOKEN_LEMMA

                    if 'offset' in c:
                        if c['offset'] == 'prior':
                            match = c['val'] in [ t[part] for t in tokens[:index] ]
                        else:
                            match = tokens[index + c['offset']][part] == c['val']
                    else:
                        match = c['val'] in [ t[part] for t in tokens ]

                    if match:
                        have_context = True
                        have_reverse = True

            if not have_context:
                if len(context) == 0:
                    have_context = True
                else:
                    for c in context:
                        part = self.TOKEN_WORD if c['type'] == self.TRIGGER_WORD else self.TOKEN_LEMMA

                        if 'offset' in c:
                            if c['offset'] == 'prior':
                                match = c['val'] in [ t[part] for t in tokens[:index] ]
                            else:
                                match = tokens[index + c['offset']][part] == c['val']
                        else:
                            match = c['val'] in [ t[part] for t in tokens ]

                        if match:
                            have_context = True
                            have_reverse = False

            return have_context, have_reverse

        return False, False

    def get_value(self, trigger, token, reverse = False):
        if reverse:
            return trigger['reverse']

        if 'ret' in trigger:
            return trigger['ret']

        if 'lookup' in trigger:
            return trigger['lookup'](self, trigger, token)

        return token[self.TOKEN_SS].name() if token[self.TOKEN_SS] else token[self.TOKEN_LEMMA]

    def normalize_age(self, trigger, token):
        if trigger['type'] == self.TRIGGER_TAG:
            val = int(token[self.TOKEN_LEMMA])

            if val < 20:
                return 'teens.n.01'
            elif val < 30:
                return 'twenties.n.01'
            elif val < 40:
                return 'thirties.n.01'
            elif val < 50:
                return 'forties.n.01'
            elif val < 60:
                return 'fifties.n.01'
            elif val < 70:
                return 'sixties.n.01'
            else:
                return 'seventies.n.01'

        elif trigger['type'] == self.TRIGGER_HYPERNYM:
            return token[self.TOKEN_SS].name()


    triggers = {
        "age": [
            dict(id=1, type=TRIGGER_TAG, val="CD", min=10, max=70, lookup=normalize_age),
            dict(id=2, type=TRIGGER_HYPERNYM, val="time_of_life.n.01", lookup=normalize_age)
        ],

        "gender": [
            dict(id=1, type=TRIGGER_LEMMA, val="man", ret="m", reverse="f"),
            dict(id=2, type=TRIGGER_LEMMA, val="husband", ret="m", reverse="f"),
            dict(id=3, type=TRIGGER_LEMMA, val="boyfriend", ret="m", reverse="f"),
            dict(id=4, type=TRIGGER_PHRASE, val=["boy", "friend"], ret="m", reverse="f"),
            dict(id=5, type=TRIGGER_LEMMA, val="woman", ret="f", reverse="m"),
            dict(id=6, type=TRIGGER_LEMMA, val="wife", ret="f", reverse="m"),
            dict(id=7, type=TRIGGER_LEMMA, val="girlfriend", ret="f", reverse="m"),
            dict(id=8, type=TRIGGER_PHRASE, val=["girl", "friend"], ret="f", reverse="m")
        ],

        "denom": [
            dict(id=1, type=TRIGGER_HYPERNYM, val="christian.n.01")
        ]
    }

    context = {
        "age": [
            dict(id=1, type=TRIGGER_WORD, val="am", triggers=[1], offset="prior"),
            dict(id=2, type=TRIGGER_WORD, val="m", triggers=[1], offset="prior"),
            dict(id=3, type=TRIGGER_WORD, val="my", triggers=[2], offset=-1)
        ],

        "gender": [
            dict(id=1, type=TRIGGER_WORD, val="my", reverse=[1,2,3,4,5,6,7,8], offset=-1),
            dict(id=2, type=TRIGGER_WORD, val="am", triggers=[1,2,3,4,5,6,7,8], offset="prior"),
            dict(id=3, type=TRIGGER_WORD, val="m", triggers=[1,2,3,4,5,6,7,8], offset="prior"),
            dict(id=4, type=TRIGGER_LEMMA, val="have", reverse=[1,2,3,4,5,6,7,8], offset="prior")
        ],

        "denom": [
            dict(id=1, type=TRIGGER_LEMMA, val="be", triggers=[1], offset="prior"),
            dict(id=2, type=TRIGGER_WORD, val="m", triggers=[1], offset="prior")
        ]
    }