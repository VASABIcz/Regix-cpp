#include <iostream>
#include <map>
#include <cassert>
#include <optional>
#include <vector>
#include <valarray>
#include <functional>
#include <memory>
#include <set>

namespace utils {
    template<typename Iterator>
    constexpr std::string_view slice(Iterator str, long index = 0, long amount = -1)  {
        assert(index >= 0);
        assert(index + amount <= str.size());
        return {str.begin()+index, str.begin()+index+(amount >= 0 ? amount : str.size()-index)};
    }

    constexpr bool isPeekChar(std::string_view str, char c)  {
        if (str.empty())
            return false;
        else
            return str[0] == c;
    }

    template<typename Func>
    constexpr bool isPeek(std::string_view str, const Func& fn)  {
        if (str.empty())
            return false;
        else
            return fn(str[0]);
    }
}

namespace lexer {
    struct Lexer {
        std::string data;
        uint index = 0;

        explicit Lexer(std::string_view data): data(data) {}

        bool isPeek(char c) const {
            if (index+1 >= data.size()) return false;

            return data[index] == c;
        }

        void consume(uint amount = 1) {
            index += amount;
        }

        bool isPeek(uint amount, const std::function<bool(std::string_view)>& f) const {
            if (index+amount > data.size()) return false;

            auto st = utils::slice(data, index, amount);
            return f(st);
        }

        bool isDone() const {
            return index >= data.size();
        }
    };
}

namespace regix {
    const std::set<char> invalidChars{'(', '[', '|', '?', '*', '+', '.', '^', ']', ')'};

    struct Regix {
        virtual long match(std::string_view source, std::vector<std::vector<std::string_view>>& matches) = 0;
        // virtual void print(int offset = 0) = 0;

        bool doesMatch(std::string_view source) {
            std::vector<std::vector<std::string_view>> ms;

            auto res = match(source, ms);
            return res == source.size();
        }
    };

    struct Any: public Regix {
        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            if (!source.empty())
                return 1;
            else
                return -1;
        }
    };

    struct Char: public Regix {
        char c;

        explicit Char(char c): c(c) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            return utils::isPeekChar(source, c) ? 1 : -1;
        }
    };

    struct Numeric: public Regix {
        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            return utils::isPeek(source, [](auto c){
                return isdigit(c);
            }) ? 1 : -1;
        }
    };

    struct Whitespace: public Regix {
        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            return utils::isPeek(source, [](auto c){
                return isspace(c);
            }) ? 1 : -1;
        }
    };

    struct Letter: public Regix {
        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            return utils::isPeek(source, [](auto c){
                return isalpha(c);
            }) ? 1 : -1;
        }
    };

    struct XAndMore: public Regix {
        std::unique_ptr<Regix> inner;
        size_t amount;

        explicit XAndMore(std::unique_ptr<Regix> inner, size_t amount): inner(std::move(inner)), amount(amount) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            long matchCount;
            long matchAmount;
            std::string_view src = source;

            while (true) {
                auto res = inner->match(src, matches);
                if (res < 0) {
                    if (matchCount >= amount)
                        return matchAmount;
                    else
                        return -1;
                }
                matchCount++;
                matchAmount += res;
                src = utils::slice(src, res);
            }
        }
    };

    struct Optional: public Regix {
        std::unique_ptr<Regix> inner;

        Optional(std::unique_ptr<Regix> inner): inner(std::move(inner)) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            auto res =  inner->match(source, matches);
            if (res < 0) {
                return 0;
            }
            return res;
        }
    };

    struct Capture: public Regix {
        std::vector<std::unique_ptr<Regix>> inner;
        long id;

        explicit Capture(std::vector<std::unique_ptr<Regix>> inner, long id) : inner(std::move(inner)), id(id) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            long matchAmount;
            auto src = source;

            for (auto& matcher : inner) {
                auto res = matcher->match(src, matches);
                if (res < 0) {
                    return -1;
                }
                matchAmount += res;
                src = utils::slice(src, res);
            }
            matches[id].push_back(utils::slice(source, 0, matchAmount));
            return matchAmount;
        }
    };

    struct Group: public Regix {
        std::vector<std::unique_ptr<Regix>> inner;

        explicit Group(std::vector<std::unique_ptr<Regix>> inner): inner(std::move(inner)) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            long matchAmount = 0;
            auto src = source;

            for (auto& matcher : inner) {
                auto res = matcher->match(src, matches);
                if (res < 0) {
                    return -1;
                }
                matchAmount += res;
                src = utils::slice(src, res);
            }

            return matchAmount;
        }
    };

    struct Or: public Regix {
        std::unique_ptr<Regix> right;
        std::unique_ptr<Regix> left;

        explicit Or(std::unique_ptr<Regix> left, std::unique_ptr<Regix> right): left(std::move(left)), right(std::move(right)) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            auto res = left->match(source, matches);

            if (res < 0) {
                return right->match(source, matches);
            }
            return res;
        }
    };

    struct Not: public Regix {
        std::unique_ptr<Regix> inner;

        explicit Not(std::unique_ptr<Regix> inner): inner(std::move(inner)) {}

        long match(std::string_view source, std::vector<std::vector<std::string_view>> &matches) override {
            return inner->match(source, matches) < 0 ? 1 : -1;
        }
    };

    bool parseSimpleRegix(lexer::Lexer& l, std::vector<std::unique_ptr<Regix>>& previous) {
        std::vector<std::unique_ptr<Regix>> buf;

        while (l.isPeek(1, [](auto c) {return !invalidChars.contains(c[0]);})) {
            auto c = l.data[l.index];
            l.consume();

            if (c == '\\') {
                if (l.isDone()) return false;

                auto p = l.data[l.index];
                l.consume();
                switch (p) {
                    case 'l':
                        buf.push_back(std::make_unique<Letter>());
                        break;
                    case 'd':
                        buf.push_back(std::make_unique<Numeric>());
                        break;
                    case 'w':
                        buf.push_back(std::make_unique<Whitespace>());
                        break;
                    default:
                        buf.push_back(std::make_unique<Char>(p));
                }
            }
            else {
                buf.push_back(std::make_unique<Char>(c));
            }
        }

        if (buf.empty()) return false;

        previous.push_back(std::make_unique<Group>(std::move(buf)));

        return true;
    }

    bool parseRegix(lexer::Lexer& l, std::vector<std::unique_ptr<Regix>>& previous, long& captureGroups) {
        auto c = l.data[l.index];

        switch (c) {
            case '(': {
                l.consume();

                auto buf = std::vector<std::unique_ptr<Regix>>();

                while (!l.isPeek(')')) {
                    parseRegix(l, buf, captureGroups);
                }
                if (!l.isPeek(')')) return false;
                l.consume();

                previous.push_back(std::make_unique<Capture>(std::move(buf), captureGroups++));

                return true;
            }
            case '[': {
                l.consume();

                auto buf = std::vector<std::unique_ptr<Regix>>();

                while (!l.isPeek(']')) {
                    parseRegix(l, buf, captureGroups);
                }
                if (!l.isPeek(']')) return false;
                l.consume();

                previous.push_back(std::make_unique<Group>(std::move(buf)));

                return true;
            }
            case '|': {
                l.consume();
                if (previous.empty()) return false;
                auto left = std::move(previous[previous.size() - 1]);
                previous.pop_back();

                auto right = std::vector<std::unique_ptr<Regix>>();
                parseRegix(l, right, captureGroups);
                if (right.size() != 1) {
                    return false;
                }

                previous.push_back(std::make_unique<Or>(std::move(left), std::move(right[0])));

                return true;
            }
            case '?': {
                l.consume();
                if (previous.empty()) {
                    return false;
                }

                auto prev = std::move(previous[previous.size() - 1]);
                previous.pop_back();

                previous.push_back(std::make_unique<Optional>(std::move(prev)));

                return true;
            }
            case '*': {
                l.consume();
                if (previous.empty()) {
                    return false;
                }

                auto prev = std::move(previous[previous.size() - 1]);
                previous.pop_back();

                previous.push_back(std::make_unique<XAndMore>(std::move(prev), 0));

                return true;
            }
            case '+': {
                l.consume();
                if (previous.empty()) {
                    return false;
                }

                auto prev = std::move(previous[previous.size() - 1]);
                previous.pop_back();

                previous.push_back(std::make_unique<XAndMore>(std::move(prev), 1));

                return true;
            }
            case '.': {
                l.consume();

                previous.push_back(std::make_unique<Any>());

                return true;
            }
            case '^': {
                auto buf = std::vector<std::unique_ptr<Regix>>();

                parseRegix(l, buf, captureGroups);

                if (buf.size() != 1) {
                    return false;
                }

                previous.push_back(std::make_unique<Not>(std::move(buf[0])));

                return true;
            }
            default:
                parseSimpleRegix(l, previous);
        }
    }

    std::unique_ptr<Regix> constructRegix(std::string_view str) {
        lexer::Lexer lexer(str);
        long captureId;
        std::vector<std::unique_ptr<Regix>> buf;

        while (!lexer.isDone()) {
            parseRegix(lexer, buf, captureId);
        }
        return std::make_unique<Group>(std::move(buf));
    }
}

int main() {
    auto reg = regix::constructRegix("uwu");

    std::cout << "finished parsing" << std::endl;

    auto res = reg->doesMatch("uwu");
    std::cout << res << std::endl;

    return 0;
}
