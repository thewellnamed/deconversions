from mapper import WordNetAffectMapper

def main():
    old_version_path = "../../wordnet/1.6/dict"
    new_version_path = "../../wordnet/3.1/dict"
    input_path = "a-synsets.xml"
    output_path = "a-synsets-3.1.xml"
    
    mapper = WordNetAffectMapper(old_version_path, new_version_path, input_path, output_path)
    mapper.translate()
    mapper.write_output()
    print "done."
    
if __name__ == '__main__':
    main()
    