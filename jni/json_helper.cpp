#include "json_helper.h"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <iomanip>

namespace AMLua
{
    namespace Json
    {
        // ==========================================
        // Lightweight Recursive Descent JSON Parser
        // ==========================================
        class Parser
        {
        public:
            Parser(const char* str, size_t len)
                : m_str(str), m_len(len), m_pos(0) {}

            bool Parse(lua_State* L, std::string& err)
            {
                SkipWhitespace();
                if (m_pos >= m_len)
                {
                    err = "Empty JSON input";
                    return false;
                }

                if (!ParseValue(L, err))
                {
                    return false;
                }

                SkipWhitespace();
                if (m_pos < m_len)
                {
                    err = "Trailing characters after JSON value at position " + std::to_string(m_pos);
                    return false;
                }

                return true;
            }

        private:
            const char* m_str;
            size_t m_len;
            size_t m_pos;

            void SkipWhitespace()
            {
                while (m_pos < m_len)
                {
                    char c = m_str[m_pos];
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                    {
                        m_pos++;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            char Peek() const
            {
                return m_pos < m_len ? m_str[m_pos] : '\0';
            }

            char Get()
            {
                return m_pos < m_len ? m_str[m_pos++] : '\0';
            }

            bool ParseValue(lua_State* L, std::string& err)
            {
                SkipWhitespace();
                char c = Peek();

                if (c == '{')
                {
                    return ParseObject(L, err);
                }
                else if (c == '[')
                {
                    return ParseArray(L, err);
                }
                else if (c == '"')
                {
                    std::string s;
                    if (!ParseString(s, err)) return false;
                    lua_pushlstring(L, s.data(), s.size());
                    return true;
                }
                else if (c == '-' || (c >= '0' && c <= '9'))
                {
                    return ParseNumber(L, err);
                }
                else if (c == 't' || c == 'f')
                {
                    return ParseBoolean(L, err);
                }
                else if (c == 'n')
                {
                    return ParseNull(L, err);
                }

                err = std::string("Unexpected character '") + c + "' at position " + std::to_string(m_pos);
                return false;
            }

            bool ParseObject(lua_State* L, std::string& err)
            {
                Get(); // consume '{'
                lua_newtable(L);

                SkipWhitespace();
                if (Peek() == '}')
                {
                    Get(); // consume '}'
                    return true;
                }

                while (m_pos < m_len)
                {
                    SkipWhitespace();
                    if (Peek() != '"')
                    {
                        err = "Expected string key in object at position " + std::to_string(m_pos);
                        return false;
                    }

                    std::string key;
                    if (!ParseString(key, err)) return false;

                    SkipWhitespace();
                    if (Get() != ':')
                    {
                        err = "Expected ':' after key in object at position " + std::to_string(m_pos);
                        return false;
                    }

                    if (!ParseValue(L, err)) return false;

                    // Set table[key] = value
                    lua_setfield(L, -2, key.c_str());

                    SkipWhitespace();
                    char c = Peek();
                    if (c == '}')
                    {
                        Get(); // consume '}'
                        return true;
                    }
                    else if (c == ',')
                    {
                        Get(); // consume ','
                    }
                    else
                    {
                        err = "Expected ',' or '}' in object at position " + std::to_string(m_pos);
                        return false;
                    }
                }

                err = "Unterminated object in JSON";
                return false;
            }

            bool ParseArray(lua_State* L, std::string& err)
            {
                Get(); // consume '['
                lua_newtable(L);

                SkipWhitespace();
                if (Peek() == ']')
                {
                    Get(); // consume ']'
                    return true;
                }

                lua_Integer index = 1;
                while (m_pos < m_len)
                {
                    if (!ParseValue(L, err)) return false;

                    // Set table[index++] = value
                    lua_rawseti(L, -2, index++);

                    SkipWhitespace();
                    char c = Peek();
                    if (c == ']')
                    {
                        Get(); // consume ']'
                        return true;
                    }
                    else if (c == ',')
                    {
                        Get(); // consume ','
                    }
                    else
                    {
                        err = "Expected ',' or ']' in array at position " + std::to_string(m_pos);
                        return false;
                    }
                }

                err = "Unterminated array in JSON";
                return false;
            }

            bool ParseString(std::string& out, std::string& err)
            {
                Get(); // consume '"'
                out.clear();

                while (m_pos < m_len)
                {
                    char c = Get();
                    if (c == '"')
                    {
                        return true;
                    }
                    else if (c == '\\')
                    {
                        if (m_pos >= m_len)
                        {
                            err = "Unterminated escape sequence in string";
                            return false;
                        }
                        char esc = Get();
                        switch (esc)
                        {
                            case '"':  out.push_back('"'); break;
                            case '\\': out.push_back('\\'); break;
                            case '/':  out.push_back('/'); break;
                            case 'b':  out.push_back('\b'); break;
                            case 'f':  out.push_back('\f'); break;
                            case 'n':  out.push_back('\n'); break;
                            case 'r':  out.push_back('\r'); break;
                            case 't':  out.push_back('\t'); break;
                            case 'u':
                            {
                                if (m_pos + 4 > m_len)
                                {
                                    err = "Invalid unicode escape sequence";
                                    return false;
                                }
                                unsigned int codepoint = 0;
                                for (int i = 0; i < 4; ++i)
                                {
                                    char h = Get();
                                    codepoint <<= 4;
                                    if (h >= '0' && h <= '9') codepoint += (h - '0');
                                    else if (h >= 'a' && h <= 'f') codepoint += (h - 'a' + 10);
                                    else if (h >= 'A' && h <= 'F') codepoint += (h - 'A' + 10);
                                    else
                                    {
                                        err = "Invalid hex in unicode escape";
                                        return false;
                                    }
                                }
                                // Encode UTF-8
                                if (codepoint <= 0x7F)
                                {
                                    out.push_back((char)codepoint);
                                }
                                else if (codepoint <= 0x7FF)
                                {
                                    out.push_back((char)(0xC0 | ((codepoint >> 6) & 0x1F)));
                                    out.push_back((char)(0x80 | (codepoint & 0x3F)));
                                }
                                else
                                {
                                    out.push_back((char)(0xE0 | ((codepoint >> 12) & 0x0F)));
                                    out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
                                    out.push_back((char)(0x80 | (codepoint & 0x3F)));
                                }
                                break;
                            }
                            default:
                                out.push_back(esc);
                                break;
                        }
                    }
                    else
                    {
                        out.push_back(c);
                    }
                }

                err = "Unterminated string in JSON";
                return false;
            }

            bool ParseNumber(lua_State* L, std::string& err)
            {
                size_t start = m_pos;
                bool isFloat = false;

                if (Peek() == '-') Get();

                while (m_pos < m_len)
                {
                    char c = Peek();
                    if (c >= '0' && c <= '9')
                    {
                        Get();
                    }
                    else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
                    {
                        isFloat = true;
                        Get();
                    }
                    else
                    {
                        break;
                    }
                }

                std::string numStr(m_str + start, m_pos - start);
                char* endPtr = nullptr;
                if (!isFloat)
                {
                    long long intVal = strtoll(numStr.c_str(), &endPtr, 10);
                    if (endPtr && *endPtr == '\0')
                    {
                        lua_pushinteger(L, (lua_Integer)intVal);
                        return true;
                    }
                }

                double dblVal = strtod(numStr.c_str(), &endPtr);
                if (endPtr && *endPtr == '\0')
                {
                    lua_pushnumber(L, (lua_Number)dblVal);
                    return true;
                }

                err = "Invalid number format: " + numStr;
                return false;
            }

            bool ParseBoolean(lua_State* L, std::string& err)
            {
                if (m_pos + 4 <= m_len && strncmp(m_str + m_pos, "true", 4) == 0)
                {
                    m_pos += 4;
                    lua_pushboolean(L, 1);
                    return true;
                }
                if (m_pos + 5 <= m_len && strncmp(m_str + m_pos, "false", 5) == 0)
                {
                    m_pos += 5;
                    lua_pushboolean(L, 0);
                    return true;
                }
                err = "Invalid boolean at position " + std::to_string(m_pos);
                return false;
            }

            bool ParseNull(lua_State* L, std::string& err)
            {
                if (m_pos + 4 <= m_len && strncmp(m_str + m_pos, "null", 4) == 0)
                {
                    m_pos += 4;
                    lua_pushnil(L);
                    return true;
                }
                err = "Invalid null at position " + std::to_string(m_pos);
                return false;
            }
        };

        // ==========================================
        // Lua-to-JSON Serializer
        // ==========================================
        static void EscapeString(std::string& out, const char* s, size_t len)
        {
            out.push_back('"');
            for (size_t i = 0; i < len; ++i)
            {
                char c = s[i];
                switch (c)
                {
                    case '"':  out.append("\\\""); break;
                    case '\\': out.append("\\\\"); break;
                    case '\b': out.append("\\b"); break;
                    case '\f': out.append("\\f"); break;
                    case '\n': out.append("\\n"); break;
                    case '\r': out.append("\\r"); break;
                    case '\t': out.append("\\t"); break;
                    default:
                        if ((unsigned char)c < 0x20)
                        {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)(unsigned char)c);
                            out.append(buf);
                        }
                        else
                        {
                            out.push_back(c);
                        }
                        break;
                }
            }
            out.push_back('"');
        }

        // Determine if Lua table is an array (1..n consecutive integers)
        static bool IsTableArray(lua_State* L, int idx, lua_Integer& maxCount)
        {
            idx = lua_absindex(L, idx);
            lua_Integer count = 0;
            lua_Integer maxKey = 0;

            lua_pushnil(L);
            while (lua_next(L, idx) != 0)
            {
                if (lua_type(L, -2) != LUA_TNUMBER || !lua_isinteger(L, -2))
                {
                    lua_pop(L, 2);
                    return false;
                }
                lua_Integer k = lua_tointeger(L, -2);
                if (k <= 0)
                {
                    lua_pop(L, 2);
                    return false;
                }
                if (k > maxKey) maxKey = k;
                count++;
                lua_pop(L, 1);
            }

            maxCount = maxKey;
            return (count == maxKey);
        }

        static bool SerializeValue(lua_State* L, int idx, std::string& out, bool pretty, int indent, int depth, std::string& err)
        {
            if (depth > 64)
            {
                err = "JSON encoding exceeded maximum depth limit (circular reference?)";
                return false;
            }

            idx = lua_absindex(L, idx);
            int t = lua_type(L, idx);

            switch (t)
            {
                case LUA_TNIL:
                    out.append("null");
                    break;

                case LUA_TBOOLEAN:
                    out.append(lua_toboolean(L, idx) ? "true" : "false");
                    break;

                case LUA_TNUMBER:
                    if (lua_isinteger(L, idx))
                    {
                        out.append(std::to_string(lua_tointeger(L, idx)));
                    }
                    else
                    {
                        double d = (double)lua_tonumber(L, idx);
                        if (std::isnan(d) || std::isinf(d))
                        {
                            out.append("null");
                        }
                        else
                        {
                            char buf[64];
                            snprintf(buf, sizeof(buf), "%.14g", d);
                            out.append(buf);
                        }
                    }
                    break;

                case LUA_TSTRING:
                {
                    size_t len = 0;
                    const char* s = lua_tolstring(L, idx, &len);
                    EscapeString(out, s, len);
                    break;
                }

                case LUA_TTABLE:
                {
                    lua_Integer arrayLen = 0;
                    bool isArray = IsTableArray(L, idx, arrayLen);

                    std::string indentStr = pretty ? std::string((indent + 1) * 2, ' ') : "";
                    std::string closingIndent = pretty ? std::string(indent * 2, ' ') : "";
                    std::string newline = pretty ? "\n" : "";
                    std::string space = pretty ? " " : "";

                    if (isArray)
                    {
                        if (arrayLen == 0)
                        {
                            out.append("[]");
                            break;
                        }

                        out.push_back('[');
                        out.append(newline);

                        for (lua_Integer i = 1; i <= arrayLen; ++i)
                        {
                            out.append(indentStr);
                            lua_rawgeti(L, idx, i);
                            if (!SerializeValue(L, -1, out, pretty, indent + 1, depth + 1, err))
                            {
                                lua_pop(L, 1);
                                return false;
                            }
                            lua_pop(L, 1);

                            if (i < arrayLen) out.push_back(',');
                            out.append(newline);
                        }

                        out.append(closingIndent);
                        out.push_back(']');
                    }
                    else
                    {
                        // Object serialization
                        out.push_back('{');
                        out.append(newline);

                        bool first = true;
                        lua_pushnil(L);
                        while (lua_next(L, idx) != 0)
                        {
                            if (!first)
                            {
                                out.push_back(',');
                                out.append(newline);
                            }
                            first = false;

                            out.append(indentStr);

                            // Key
                            if (lua_type(L, -2) == LUA_TSTRING)
                            {
                                size_t kLen = 0;
                                const char* k = lua_tolstring(L, -2, &kLen);
                                EscapeString(out, k, kLen);
                            }
                            else
                            {
                                std::string k = lua_tostring(L, -2);
                                EscapeString(out, k.data(), k.size());
                            }

                            out.push_back(':');
                            out.append(space);

                            // Value
                            if (!SerializeValue(L, -1, out, pretty, indent + 1, depth + 1, err))
                            {
                                lua_pop(L, 2);
                                return false;
                            }

                            lua_pop(L, 1); // pop value, leave key
                        }

                        if (!first) out.append(newline);
                        out.append(closingIndent);
                        out.push_back('}');
                    }
                    break;
                }

                default:
                    // Functions, userdata, threads become null
                    out.append("null");
                    break;
            }

            return true;
        }

        // ==========================================
        // Lua API Callbacks
        // ==========================================

        // Json.Decode(str) / Json.Parse(str) -> table/val, error
        static int Lua_Json_Decode(lua_State* L)
        {
            size_t len = 0;
            const char* jsonStr = luaL_checklstring(L, 1, &len);

            Parser parser(jsonStr, len);
            std::string err;
            if (!parser.Parse(L, err))
            {
                lua_pushnil(L);
                lua_pushstring(L, err.c_str());
                return 2;
            }

            return 1;
        }

        // Json.Encode(val, [pretty]) / Json.Stringify(val, [pretty]) -> jsonStr, error
        static int Lua_Json_Encode(lua_State* L)
        {
            if (lua_gettop(L) < 1)
            {
                return luaL_error(L, "Json.Encode: requires at least 1 argument");
            }

            bool pretty = false;
            if (lua_gettop(L) >= 2 && lua_isboolean(L, 2))
            {
                pretty = lua_toboolean(L, 2);
            }

            std::string out;
            std::string err;
            if (!SerializeValue(L, 1, out, pretty, 0, 0, err))
            {
                lua_pushnil(L);
                lua_pushstring(L, err.c_str());
                return 2;
            }

            lua_pushlstring(L, out.data(), out.size());
            return 1;
        }

        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            lua_pushcfunction(L, Lua_Json_Decode);
            lua_setfield(L, -2, "Decode");

            lua_pushcfunction(L, Lua_Json_Decode);
            lua_setfield(L, -2, "Parse");

            lua_pushcfunction(L, Lua_Json_Encode);
            lua_setfield(L, -2, "Encode");

            lua_pushcfunction(L, Lua_Json_Encode);
            lua_setfield(L, -2, "Stringify");

            // Register Json globally and in AMLua
            lua_pushvalue(L, -1);
            lua_setglobal(L, "Json");

            lua_getglobal(L, "AMLua");
            if (lua_istable(L, -1))
            {
                lua_pushvalue(L, -2);
                lua_setfield(L, -2, "Json");
            }
            lua_pop(L, 2); // pop AMLua and Json
        }
    }
}
