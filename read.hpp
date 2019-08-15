#pragma once

#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <regex>

namespace zachlisp {

    // zachlisp::token
    namespace token {

        // zachlisp::token::type
        namespace type {

        enum Type {WHITESPACE, SPECIAL_CHARS, SPECIAL_CHAR, STRING, COMMENT, NUMBER, SYMBOL};

        }

        // zachlisp::token::value
        namespace value {

        using Value = std::variant<bool, char, long, double, std::string>;

        enum Type {BOOL, CHAR, LONG, DOUBLE, STRING};

        }

    const std::regex REGEX(
        "([\\s,]+)|"                   // type::WHITESPACE
        "(~@|#\\{)|"                   // type::SPECIAL_CHARS
        "([\\[\\]{}()\'`~^@])|"        // type::SPECIAL_CHAR
        "(\"(?:\\\\.|[^\\\\\"])*\"?)|" // type::STRING
        "(;.*)|"                       // type::COMMENT
        "(\\d+\\.?\\d*)|"              // type::NUMBER
        "([^\\s\\[\\]{}(\'\"`,;)]+)"   // type::SYMBOL
    );

    struct Token {
        value::Value value;
        type::Type type;
        int line;
        int column;

        Token(value::Value v, type::Type t, int l, int c) : value(v), type(t), line(l), column(c) {}

        bool operator==(const Token & t) const {
            return (value == t.value) && (type == t.type);
        }
    };

    }

    // zachlisp::form
    namespace form {

    struct Special {
        std::string name;
        std::string message;
        std::optional<token::Token> token;

        Special(std::string n, std::string m, std::optional<token::Token> t) : name(n), message(m), token(t) {}

        bool operator==(const Special & re) const {
            return (!message.compare(re.message)) && (token == re.token);
        }
    };

    struct FormWrapper;
    class FormWrapperHash;
    class FormWrapperEquality;

    using FormWrapperMap = std::unordered_map<FormWrapper, FormWrapper, FormWrapperHash, FormWrapperEquality>;
    using FormWrapperSet = std::unordered_set<FormWrapper, FormWrapperHash, FormWrapperEquality>;

    using Form = std::variant<
        Special,
        token::Token,
        std::list<FormWrapper>,
        std::vector<FormWrapper>,
        // maps and sets must be stored with a layer of indirection
        // to satisfy the type syetem.
        // their content needs to be hashable
        // and that isn't implemented until later...
        std::shared_ptr<FormWrapperMap>,
        std::shared_ptr<FormWrapperSet>
    >;

    enum Type {SPECIAL, TOKEN, LIST, VECTOR, MAP, SET};

    std::size_t hash(const FormWrapper & fw);
    bool equals(const FormWrapper & fw1, const FormWrapper & fw2);

    }

// see: https://stackoverflow.com/a/38140932

inline void hash_combine(std::size_t& seed) { }

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    hash_combine(seed, rest...);
}

}

namespace std {

template <> struct hash<zachlisp::token::Token> {
    size_t operator()(const zachlisp::token::Token & x) const {
        return std::hash<zachlisp::token::value::Value>()(x.value);
    }
};

template <> struct hash<zachlisp::form::Special> {
    size_t operator()(const zachlisp::form::Special & x) const {
        return std::hash<std::string>()(x.message);
    }
};

template <> struct hash<zachlisp::form::FormWrapper> {
    size_t operator()(const zachlisp::form::FormWrapper & x) const {
        return zachlisp::form::hash(x);
    }
};

}

namespace zachlisp {

    // zachlisp::token
    namespace token {

    value::Value parse(std::string value, type::Type type) {
        switch (type) {
            case type::SPECIAL_CHAR:
                return value[0];
            case type::NUMBER:
                if (value.find('.') == std::string::npos) {
                    return std::stol(value);
                } else {
                    return std::stod(value);
                }
            case type::SYMBOL:
                if (value == "true") {
                    return true;
                } else if (value == "false") {
                    return false;
                }
        }
        return value;
    }

    std::list<Token> tokenize(std::string input) {
        std::sregex_iterator begin(input.begin(), input.end(), REGEX);
        std::sregex_iterator end;

        std::list<Token> tokens;

        int line = 1;

        for (auto it = begin; it != end; ++it) {
            std::smatch match = *it;
            for(auto i = 1; i < match.size(); ++i){
               if (!match[i].str().empty()) {
                   std::string value_str = match.str();
                   type::Type type = static_cast<type::Type>(i-1);
                   value::Value value = parse(value_str, type);
                   int column = match.position() + 1;
                   tokens.push_back(Token{value, type, line, column});
                   line += std::count(value_str.begin(), value_str.end(), '\n');
                   break;
               }
            }
        }

        return tokens;
    }

    }

    // zachlisp::form
    namespace form {

    struct FormWrapper {
        Form form;

        FormWrapper(Form f) : form(f) {}

        bool operator==(const FormWrapper & fw) const {
            return equals(*this, fw);
        }
    };

    class FormWrapperHash {
    public:
        std::size_t operator()(const FormWrapper & fw) const {
            return hash(fw);
        }
    };

    class FormWrapperEquality {
    public:
        bool operator()(const FormWrapper & fw1, const FormWrapper & fw2) const {
            return equals(fw1, fw2);
        }
    };
    
    template <class T>
    std::size_t hash(T list) {
        std::size_t ret = 0;
        for (auto item : list) {
            hash_combine(ret, item);
        }
        return ret;
    }

    std::size_t hash(FormWrapperSet set) {
        std::vector<std::size_t> hashes;
        for (auto item : set) {
            hashes.push_back(hash(item));
        }

        std::sort(hashes.begin(), hashes.end());

        std::size_t ret = 0;
        for (auto hash : hashes) {
            hash_combine(ret, hash);
        }
        return ret;
    }

    std::size_t hash(FormWrapperMap map) {
        std::vector<std::size_t> hashes;
        for (auto item : map) {
            std::size_t hash = 0;
            hash_combine(hash, item.first, item.second);
            hashes.push_back(hash);
        }

        std::sort(hashes.begin(), hashes.end());

        std::size_t ret = 0;
        for (auto hash : hashes) {
            hash_combine(ret, hash);
        }
        return ret;
    }

    std::size_t hash(const FormWrapper & fw) {
        switch (fw.form.index()) {
            case SPECIAL:
                return std::hash<form::Special>()(std::get<form::Special>(fw.form));
            case TOKEN:
                return std::hash<token::Token>()(std::get<token::Token>(fw.form));
            case LIST:
                return hash<std::list<FormWrapper>>(std::get<std::list<FormWrapper>>(fw.form));
            case VECTOR:
                return hash<std::vector<FormWrapper>>(std::get<std::vector<FormWrapper>>(fw.form));
            case MAP:
                return hash(*std::get<std::shared_ptr<FormWrapperMap>>(fw.form));
            case SET:
                return hash(*std::get<std::shared_ptr<FormWrapperSet>>(fw.form));
        }
        return 0;
    }

    bool equals(const FormWrapper & fw1, const FormWrapper & fw2) {
        // TODO: equality checking probably shouldn't rely on hashes
        return hash(fw1) == hash(fw2);
    }

    }

const std::unordered_map<std::variant<char, std::string>, std::string> SYMBOL_TO_NAME = {
    {'\'', "quote"},
    {'`', "quasiquote"},
    {'~', "unquote"},
    {'@', "deref"},
    {'^', "with-meta"},
    {"~@", "splice-unquote"}
};

const std::unordered_map<std::variant<char, std::string>, form::Type> DELIMITER_TO_TYPE = {
    {'(', form::LIST},
    {'[', form::VECTOR},
    {'{', form::MAP},
    {"#{", form::SET},
};

const std::unordered_map<form::Type, char> TYPE_TO_DELIMITER = {
    {form::LIST, ')'},
    {form::VECTOR, ']'},
    {form::MAP, '}'},
    {form::SET, '}'}
};

std::pair<form::Form, std::list<token::Token>::const_iterator> read_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it);
std::optional<std::pair<form::Form, std::list<token::Token>::const_iterator> > read_useful_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it);
std::optional<std::pair<token::Token, std::list<token::Token>::const_iterator> > read_useful_token(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it);

form::Form list_to_vector(const std::list<form::FormWrapper> list) {
    return std::vector<form::FormWrapper> {
        std::make_move_iterator(std::begin(list)),
        std::make_move_iterator(std::end(list))
    };
}

form::Form list_to_map(const std::list<form::FormWrapper> list) {
    auto m = std::make_shared<form::FormWrapperMap>(form::FormWrapperMap{});
    form::FormWrapperMap::const_iterator map_it = m->begin();
    std::list<form::FormWrapper>::const_iterator list_it = list.begin();
    while (list_it != list.end()) {
        auto key = *list_it;
        ++list_it;
        if (list_it == list.end()) {
            return form::Special{"ReaderError", "Map must contain even number of forms", std::nullopt};
        } else {
            auto val = *list_it;
            ++list_it;
            m->insert(map_it, std::pair(key, val));
        }
    }
    return m;
}

form::Form list_to_set(const std::list<form::FormWrapper> list) {
    auto s = std::make_shared<form::FormWrapperSet>(form::FormWrapperSet{});
    form::FormWrapperSet::const_iterator it = s->begin();
    for (auto item : list) {
        s->insert(it, item);
    }
    return s;
}

std::pair<form::Form, std::list<token::Token>::const_iterator> read_coll(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, form::Type form_type) {
    char end_delimiter = TYPE_TO_DELIMITER.at(form_type);
    std::list<form::FormWrapper> forms;
    while (auto ret_opt = read_useful_token(tokens, it)) {
        auto ret = ret_opt.value();
        auto token = ret.first;
        it = ret.second;
        if (token.type == token::type::SPECIAL_CHAR) {
            char c = std::get<char>(token.value);
            if (c == end_delimiter) {
                switch (form_type) {
                    case form::VECTOR:
                        return std::make_pair(list_to_vector(forms), ++it);
                    case form::MAP:
                        return std::make_pair(list_to_map(forms), ++it);
                    case form::SET:
                        return std::make_pair(list_to_set(forms), ++it);
                    default:
                        return std::make_pair(forms, ++it);
                }
            } else {
                switch (c) {
                    case ')':
                    case ']':
                    case '}':
                        return std::make_pair(form::Special{"ReaderError", "Unmatched delimiter: " + std::string(1, c), token}, tokens->end());
                }
            }
        }
        auto ret2 = read_form(tokens, it);
        forms.push_back(form::FormWrapper{ret2.first});
        it = ret2.second;
    }
    return std::make_pair(form::Special{"ReaderError", "EOF: no " + std::string(1, end_delimiter) + " found", std::nullopt}, tokens->end());
}

std::pair<form::Form, std::list<token::Token>::const_iterator> expand_quoted_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, token::Token token) {
    if (auto ret_opt = read_useful_form(tokens, it)) {
        auto ret = ret_opt.value();
        std::list<form::FormWrapper> list {
            form::FormWrapper{token},
            ret.first
        };
        return std::make_pair(list, ret.second);
    } else {
        return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after quote", token}, tokens->end());
    }
}

std::pair<form::Form, std::list<token::Token>::const_iterator> expand_meta_quoted_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, token::Token token) {
    if (auto ret_opt = read_useful_form(tokens, it)) {
        auto ret = ret_opt.value();
        if (auto ret_opt2 = read_useful_form(tokens, ret.second)) {
            auto ret2 = ret_opt2.value();
            std::list<form::FormWrapper> list {
                form::FormWrapper{token},
                ret2.first,
                ret.first
            };
            return std::make_pair(list, ret2.second);
        } else {
            return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after metadata", token}, tokens->end());
        }
    } else {
        return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after ^", token}, tokens->end());
    }
}

std::pair<form::Form, std::list<token::Token>::const_iterator> read_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it) {
    auto token = *it;
    switch (token.type) {
        case token::type::SPECIAL_CHARS:
            {
                std::string s = std::get<std::string>(token.value);
                if (s == "#{") {
                    return read_coll(tokens, ++it, DELIMITER_TO_TYPE.at(s));
                } else if (s == "~@") {
                    return expand_quoted_form(tokens, ++it, token::Token{SYMBOL_TO_NAME.at(s), token::type::SYMBOL, token.line, token.column});
                }
                break;
            }
        case token::type::SPECIAL_CHAR:
            {
                char c = std::get<char>(token.value);
                switch (c) {
                    case '(':
                    case '[':
                    case '{':
                        return read_coll(tokens, ++it, DELIMITER_TO_TYPE.at(c));
                    case ')':
                    case ']':
                    case '}':
                        return std::make_pair(form::Special{"ReaderError", "Unmatched delimiter: " + std::string(1, c), token}, tokens->end());
                    case '\'':
                    case '`':
                    case '~':
                    case '@':
                        return expand_quoted_form(tokens, ++it, token::Token{SYMBOL_TO_NAME.at(c), token::type::SYMBOL, token.line, token.column});
                    case '^':
                        return expand_meta_quoted_form(tokens, ++it, token::Token{SYMBOL_TO_NAME.at(c), token::type::SYMBOL, token.line, token.column});
                }
                break;
            }
        case token::type::STRING:
            {
                std::string s = std::get<std::string>(token.value);
                if (s.size() < 2 || s.back() != '"') {
                    return std::make_pair(form::Special{"ReaderError", "EOF: unbalanced quote", token}, tokens->end());
                } else {
                    token.value = s.substr(1, s.size() - 2);
                }
                break;
            }
    }
    return std::make_pair(token, ++it);
}

std::optional<std::pair<token::Token, std::list<token::Token>::const_iterator> > read_useful_token(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it) {
    while (it != tokens->end()) {
        auto token = *it;
        switch (token.type) {
            case token::type::WHITESPACE:
            case token::type::COMMENT:
                ++it;
                break;
            default:
                return std::make_pair(token, it);
        }
    }
    return std::nullopt;
}

std::optional<std::pair<form::Form, std::list<token::Token>::const_iterator> > read_useful_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it) {
    if (auto ret_opt = read_useful_token(tokens, it)) {
        auto ret = ret_opt.value();
        return read_form(tokens, ret.second);
    } else {
        return std::nullopt;
    }
}

std::list<form::Form> read_forms(const std::list<token::Token> *tokens) {
    std::list<form::Form> forms;
    std::list<token::Token>::const_iterator it = tokens->begin();
    while (auto ret_opt = read_useful_form(tokens, it)) {
        auto ret = ret_opt.value();
        forms.push_back(ret.first);
        it = ret.second;
    }
    return forms;
}

std::list<form::Form> read(const std::string input) {
    auto tokens = token::tokenize(input);
    auto forms = read_forms(&tokens);
    return forms;
}

}
