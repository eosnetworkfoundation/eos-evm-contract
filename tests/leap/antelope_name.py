def convert_name_to_value(name):
    def value_of_char(c: str):
        assert len(c) == 1
        if c >= 'a' and c <= 'z':
            return ord(c) - ord('a') + 6
        if c >= '1' and c <= '5':
            return ord(c) - ord('1') + 1
        if c == '.':
            return 0
        raise ValueError("invalid Antelope name: character '{0}' is not allowed".format(c))
    
    name_length = len(name)
    
    if name_length > 13:
        raise ValueError("invalid Antelope name: cannot exceed 13 characters")
    
    thirteen_char_value = 0
    if name_length == 13:
        thirteen_char_value = value_of_char(name[12])
        if thirteen_char_value > 15:
            raise ValueError("invalid Antelope name: 13th character cannot be letter past j")
    
    normalized_name = name[:12].ljust(12,'.') # truncate/extend to at exactly 12 characters since the 13th character is handled differently
    
    def convert_to_value(str):
        value = 0
        for c in str:
            value <<= 5
            value |= value_of_char(c)
            
        return value        
    
    return (convert_to_value(normalized_name) << 4) | thirteen_char_value
