//============================================================================
// Name        : crunchx.cpp
// Author      : vincent
// Version     :
// Copyright   : all right reserved
// Description : Crunchx can create a wordlist based on criteria you specify
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include <map>
#include <list>
#include <string>
#include <algorithm>
using namespace std;

static const int    MAX_FILE_SIZE               = 1024*1024*2; //2M
static const char   DEFAULT_RULES_FILE_NAME[]   = "crunchx.rul";
static const char   DEFAULT_RULES[]             = "NUM:'0','1','2','3','4','5','6','7','8','9'\n"
"LITER_LOWER:'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'\n"
"LITER_UPPER:'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'\n"
"LITER:LITER_LOWER\n"
"LITER:LITER_UPPER\n"
"WORD:LITER,NUM\n"
"PRODUCER: WORD WORD WORD WORD WORD WORD WORD WORD\n";

static const char HELP_TXT[] = "Crunchx version 1.0\n"
"Crunchx can create a wordlist based on criteria you specify.  The output from crunchx can be sent to the screen, file, or to another program.\n"
"Usage: crunchx [options]\n"
"options:\n"
"-h         show this help text\n"
"-l         create default rule file, named \"crunchx.rul\"\n"
"-f file    use the specified rule file,if not specified will use default rule file:\"crunchx.rul\"\n";

static void printTab( int count )
{
    while( count-- > 0 )
        printf( " " );
}

class ErrorMan{
public:
    enum ErrorCode{ eOk, eCanNotOpenFile, eFileToLarge, eInvalidParam, eReadFileErr, eWriteFileErr, eInvalidRules, eNosuchProducer, eInvalidGrammar, eMisc };
    ErrorMan()
    {
        m_code = 0;
    }

    static void setError( int errCode, const string& errMesg )
    {
        sm_errorMan.m_code = errCode;
        sm_errorMan.m_mesg = errMesg;
    }

    static bool isErrorOccured()
    {
        return ( sm_errorMan.m_code != 0 );
    }

    static const string& errorMessage()
    {
        return sm_errorMan.m_mesg;
    }

    static int errorCode()
    {
        return sm_errorMan.m_code;
    }

protected:
    int     m_code;
    string  m_mesg;
    static  ErrorMan sm_errorMan;
};

class Producer;
class Token{
public:
    enum Type{ eConfused, eTerminater, eProductor };
    Token( const string& txt, Type type ) : m_token( txt ), m_type( type ), m_producer( NULL )
    {
    }
    Token( Producer* producer )
    {
        setProducer( producer );
    }
    ~Token()
    {

    }

    string token()
    {
        return m_token;
    }

    Type type()
    {
        return m_type;
    }

    void setType( Type t )
    {
        m_type = t;
    }

    Producer* producer()
    {
        return m_producer;
    }
    void setProducer( Producer* p );

protected:
    string	m_token;
    Type    m_type;
    Producer* m_producer;
};

class ProductRule{
public:
    typedef list<Token> Items;
    ProductRule()
    {
    }

    ProductRule( const ProductRule& p )
    {
        Items::const_iterator iter;
        for( iter = p.m_items.begin(); iter != p.m_items.end(); ++iter )
            addToken( *iter );
    }

    const ProductRule& operator = ( const ProductRule& p )
    {
        Items::const_iterator iter;
        for( iter = p.m_items.begin(); iter != p.m_items.end(); ++iter )
            addToken( *iter );
        return *this;
    }

    ~ProductRule()
    {
    }

    inline void reset()
    {
        m_items.clear();
    }

    inline void addToken( const Token& t )
    {
        m_items.push_back( t );
    }

    inline bool isValid() const
    {
        return ( m_items.size() != 0 );
    }

    inline Items& items()
    {
        return m_items;
    }

protected:
    Items   m_items;
};

class Producer{
public:
    typedef list<ProductRule> Rules;
    Producer()
    {
        m_isConfused = true;
    }

    Rules& rules()
    {
        return m_rules;
    }

    inline void addRule( const ProductRule& rule )
    {
        m_rules.push_back( rule );
    }

    const string& name() const
    {
        return m_name;
    }

    void setName( const string& name )
    {
        m_name = name;
    }

    bool isConfused()
    {
        return m_isConfused;
    }

    bool confusedProductors( list<Producer*>& confusedList )
    {
        Rules::iterator ruleIter = m_rules.begin();
        for ( ; ruleIter != m_rules.end(); ++ruleIter )
        {
            ProductRule& rule = *ruleIter;
            ProductRule::Items::iterator itemIter = rule.items().begin();
            for ( ; itemIter != rule.items().end(); ++itemIter )
            {
                Token& token = *itemIter;
                if( !confirmType( token, confusedList ) )
                    return false;
            }
        }
        if( confusedList.empty() )
            m_isConfused = false;
        else
            confusedList.unique();
        return true;
    }

    static inline void updateProductorMap( const string& name, const ProductRule& rule )
    {
        ProductorMap::iterator iter = m_mapProducer.find( name );
        if( iter != m_mapProducer.end() )
        {
            iter->second.addRule( rule );
        }else
        {
            m_mapProducer[ name ].setName( name );
            m_mapProducer[ name ].addRule( rule );
        }
    }

protected:
    bool confirmType( Token& t, list<Producer*>& confusedList )
    {
        if( t.type() == Token::eTerminater )
            return true;
        if( t.type() == Token::eConfused ) //eConfused must be eProductor
        {
            ProductorMap::iterator producerIter = m_mapProducer.find( t.token() );
            if( producerIter == m_mapProducer.end() )
            {
                string err = "can not found producer:" + t.token();
                ErrorMan::setError( ErrorMan::eNosuchProducer, err );
                //t.setType( Token::eTerminater );
                return false;
            }else
            {
                Producer* producer = &( producerIter->second );
                t.setProducer( producer );
                if( producer->isConfused() )
                {
                    if( find( confusedList.begin(), confusedList.end(), producer ) == confusedList.end() )
                        confusedList.push_back( producer );
                }
            }
        }
        return true;
    }

protected:
    string          m_name;
    Rules           m_rules;
    bool            m_isConfused;
public:
    typedef map< string, Producer> ProductorMap;
    static ProductorMap m_mapProducer;
};


class ProducerReference;
class TokenReference
{
public:
    TokenReference( Token* token );
    ~TokenReference();

    ProducerReference* producerReference()
    {
        return m_producer;
    }
    Token* token()
    {
        return m_token;
    }
    bool product( string & result );
    void reset();
    bool atEnd();
    void makeNextProduct();
protected:
    bool                m_atEnd;
    Token*              m_token;
    ProducerReference*  m_producer;
};

class RuleReference{
public:
    RuleReference( ProductRule* rule )
    {
        m_rule = rule;
        ProductRule::Items::iterator iter = rule->items().begin();
        for( ; iter != rule->items().end(); ++iter )
        {
            TokenReference* ref = new TokenReference( &(*iter) );
            m_tokens.push_back( ref );
        }
        m_atEnd = false;
    }

    ~RuleReference()
    {
        list<TokenReference*>::iterator iter = m_tokens.begin();
        for( ;iter != m_tokens.end(); ++iter )
            delete *iter;
    }

    bool atEnd()
    {
        return m_atEnd;
    }

    void reset()
    {
        m_atEnd = false;
    }

    bool product( string & result )
    {
        if( atEnd() )
            return false;

        list<TokenReference*>::iterator iter = m_tokens.begin();
        for ( ;iter != m_tokens.end(); ++iter )
        {
            TokenReference* token = *iter;
            if( !token->product( result ) )
                return false;
        }
        return true;
    }

    void makeNextProduct()
    {
        if( m_atEnd )
            return;

        m_atEnd = true;
        list<TokenReference*>::iterator iter = m_tokens.begin();
        for( ;iter != m_tokens.end(); ++iter )
        {
            TokenReference* tokenRef = *iter;
            tokenRef->makeNextProduct();
            if( tokenRef->atEnd() )
                tokenRef->reset();
            else
            {
                m_atEnd = false;
                break;
            }
        }
    }
    void trace( int deep );
protected:
    ProductRule*  m_rule;
    list<TokenReference*> m_tokens;
    bool m_atEnd;
};

class ProducerReference{
public:
    ProducerReference( Producer* producer )
    {
        assert( producer != NULL );
        m_producer = producer;
        Producer::Rules::iterator iter = producer->rules().begin();
        for ( ;iter != producer->rules().end(); ++iter )
        {
            RuleReference* ref = new RuleReference( &( *iter ) );
            m_rules.push_back( ref );
        }
        m_iter = m_rules.begin();
    }

    ~ProducerReference()
    {
        list<RuleReference*>::iterator iter = m_rules.begin();
        for( ;iter != m_rules.end(); ++iter )
            delete *iter;
    }

    inline bool atEnd()
    {
        return ( m_iter == m_rules.end() );
    }

    bool isVvalid()
    {
        return ( m_producer && !atEnd()  );
    }

    bool product( string & result )
    {
        RuleReference* rule = *m_iter;
        return rule->product( result );
    }

    void makeNextProduct()
    {
        if( atEnd() )
            return;

        RuleReference* rule = *m_iter;
        rule->makeNextProduct();
        if( rule->atEnd() )
        {
            rule->reset();
            ++m_iter;
        }
    }

    void reset()
    {
        m_iter = m_rules.begin();
        list<RuleReference*>::iterator iter = m_rules.begin();
        for ( ;iter != m_rules.end(); ++iter )
            (*iter)->reset();
    }

    void trace( int deep )
    {
        printTab( deep );
        printf( "PRODUCT[%s]\n",m_producer->name().c_str() );
        list<RuleReference*>::iterator iter = m_rules.begin();
        for( ;iter != m_rules.end(); ++iter )
            (*iter)->trace( deep + 1 );
    }
protected:
    Producer*  m_producer;
    list<RuleReference*> m_rules;
    list<RuleReference*>::iterator m_iter;
};

void RuleReference::trace( int deep )
{
    list<TokenReference*>::iterator iter = m_tokens.begin();
    for( ;iter != m_tokens.end(); ++iter )
    {
        TokenReference* token = *iter;
        ProducerReference* product = token->producerReference();
        if( product )
            product->trace( deep + 1 );
        else
        {
            printTab( deep );
            printf( "%s\n", token->token()->token().c_str() );
        }
    }
}

TokenReference::TokenReference( Token* token )
{
    m_atEnd = false;
    m_token = token;
    if( token->type() == Token::eProductor )
    {
        m_producer = new ProducerReference( token->producer() );
    }else
        m_producer = NULL;
}

TokenReference::~TokenReference()
{
    if( m_producer )
        delete m_producer;
}

bool TokenReference::product( string & result )
{
    if( m_producer )
        return m_producer->product( result );
    result += m_token->token();
    return true;
}

void TokenReference::reset()
{
    if( m_producer )
        return m_producer->reset();
    m_atEnd = false;
}

bool TokenReference::atEnd()
{
    if( m_producer )
        return m_producer->atEnd();
    return m_atEnd;
}

void TokenReference::makeNextProduct()
{
    if( m_producer )
        return m_producer->makeNextProduct();
    m_atEnd = true;
}

ErrorMan ErrorMan::sm_errorMan;
Producer::ProductorMap Producer::m_mapProducer;

void Token::setProducer( Producer* p )
{
    m_token = p->name();
    m_type = eProductor;
    m_producer = p;
}

class Crunchx{
public:
    Crunchx() : m_rules(0),m_rulesBuffSize(0),m_rulesLength(0),m_status( eIdle ),
        m_rulesAnalysisIndex(NULL),m_lineCount(0),m_mainProductor( NULL )
    {
    }

    ~Crunchx()
    {
        cleanup();
    }


    ErrorMan::ErrorCode openRulesFile( const char* fileName )
    {
        FILE* fRules = fopen( fileName, "r" );
        if( fRules == NULL )
            return ErrorMan::eCanNotOpenFile;

        m_status = eIdle;
        ErrorMan::ErrorCode err = ErrorMan::eOk;
        fseek( fRules, 0, SEEK_END );
        size_t size = ftell( fRules );
        if( size <= MAX_FILE_SIZE )
        {
            if( size > m_rulesBuffSize )
            {
                delete[] m_rules;
                m_rulesBuffSize = size + 1;
                m_rules = new char[ m_rulesBuffSize ];
            }
            fseek( fRules, 0, SEEK_SET );
            size_t readSize = fread( m_rules, 1, size, fRules );
            if( readSize > size )
            {
                m_rulesLength = 0;
                err = ErrorMan::eReadFileErr;
            }else
            {
                m_rulesLength = readSize;
                m_rules[ readSize ] = 0;
                m_rulesAnalysisIndex = m_rules;
                m_status = eBuffIsSet;
            }
        }else
            err = ErrorMan::eFileToLarge;
        fclose( fRules );

        return err;
    }

    ErrorMan::ErrorCode saveRulesFile( const char* fileName )
    {
        if( m_status == eIdle )
            return ErrorMan::eInvalidRules;
        FILE* fRules = fopen( fileName, "w+" );
        if( fRules == NULL )
            return ErrorMan::eCanNotOpenFile;
        size_t writeCount = fwrite( m_rules, 1, m_rulesLength, fRules );
        fclose( fRules );
        if( writeCount == m_rulesLength )
            return ErrorMan::eOk;
        else
            return ErrorMan::eWriteFileErr;
    }

    ErrorMan::ErrorCode setRules( const char* rules, const size_t length )
    {
        if( length > MAX_FILE_SIZE )
            return ErrorMan::eFileToLarge;

        if( length > m_rulesBuffSize )
        {
            delete[] m_rules;
            m_rulesBuffSize = length + 1;
            m_rules = new char[ m_rulesBuffSize ];
        }
        memcpy( m_rules, rules, length );
        m_rules[ length ] = 0;
        m_rulesAnalysisIndex = m_rules;
        m_status = eBuffIsSet;
        m_rulesLength = length;
        return ErrorMan::eOk;
    }

    void cleanup()
    {
        if( m_rules )
            delete[] m_rules;
        m_rules = 0;
        m_rulesBuffSize = 0;
    }

    bool atEnd()
    {
        assert( m_mainProductor != NULL );
        return ( m_mainProductor == NULL || m_mainProductor->atEnd() );
    }

    bool analysis()
    {
        m_lineCount = 0;
        while ( analysisProducer() )
            ;

        if( ErrorMan::isErrorOccured() )
            return false;

        return analysisDependence();
    }

    void makeNextProduct()
    {
        if( atEnd() )
            return;
        return m_mainProductor->makeNextProduct();
    }

    bool product( string& str )
    {
        if( atEnd() )
            return false;
        str.clear();
        return m_mainProductor->product( str );
    }

protected:
    void cutLine( char* line )
    {
        while( *line != '\0' )
        {
            char c = *line;
            if( c == '\r' || c == '\n' )
            {
                *line = '\0';
                return;
            }
            ++line;
        }
    }

    bool analysisProducer()
    {
        string name;
        if( ErrorMan::isErrorOccured() ||
            m_rulesAnalysisIndex == NULL || m_rulesAnalysisIndex >= m_rules + m_rulesLength )
        {

            return false;
        }

        ++m_lineCount;
        bool invalidGrammar = false;
        ProductRule rule;
        Token::Type tokenType = Token::eConfused;
        char* beginLine = m_rulesAnalysisIndex;
        string element;
        char c;
        char pair = 0;
        enum ReadStatus{ eReadName, eReadElement, eReadStringPair, eReadEndElement };
        ReadStatus s = eReadName;
        while( ( c = *( m_rulesAnalysisIndex++ ) ) != 0 && c != '\n')
        {
            if( c == '\r' || c == '\t' )
                continue;
            bool processRule = false;
            if( eReadName == s )
            {
                if( c == ':' )
                {
                    s = eReadElement;
                }else if( c == '#' )
                {
                    if( name.empty() )
                    {
                        while( ( c = *( m_rulesAnalysisIndex++ ) ) != 0 && c != '\n')
                            ;//skip comment line
                        return true;
                    }
                }else if( c != ' ' )
                    name.push_back( c );
            }else if( eReadElement == s )
            {
                if( c == '\'' || c == '"' )
                {
                    pair = c;
                    s = eReadStringPair;
                    tokenType = Token::eTerminater;
                }else if( c == ',' )
                {
                    processRule = true;
                }else
                {
                    if( element.empty() && c == ' ' )//skip space
                        continue;

                    if( c != ' ' )
                    {
                        element.push_back( c );
                        continue;
                    }

                    if( element.size() )
                    {
                        rule.addToken( Token( element, tokenType ) );
                        tokenType = Token::eConfused;
                        element.clear();
                    }
                }
            }else if( eReadStringPair == s )
            {
                if( c == pair )
                {
                    pair = 0;
                    s = eReadEndElement;
                }else
                    element.push_back( c );
            }else if( eReadEndElement == s )
            {
                if( c == ' ' )
                    continue;
                if( c == ',' )
                {
                    processRule = true;
                }else
                {
                    rule.addToken( Token( element, tokenType ) );
                    tokenType = Token::eConfused;
                    element.clear();
                    element.push_back( c );
                }
                s = eReadElement;
            }
            if( processRule )
            {
                if( !element.empty() )
                    rule.addToken( Token( element, tokenType ) );
                if( !rule.isValid() )
                {
                    invalidGrammar = true;
                    break;
                }
                Producer::updateProductorMap( name, rule );
                rule.reset();
                element.clear();
                tokenType = Token::eConfused;
            }
        }

        if( eReadName == s && name.empty() ) //empty line
            return true;
        else if( ( eReadElement != s && eReadEndElement != s ) || invalidGrammar )
        {
            cutLine( beginLine );
            string err = "invalid grammar in line:";
            err += beginLine;
            ErrorMan::setError( ErrorMan::eInvalidGrammar, err );
            return false;
        }

        if( !element.empty() )
            rule.addToken( Token( element, tokenType ) );
        if( rule.isValid() )
            Producer::updateProductorMap( name, rule );
        return true;
    }

    bool analysisDependence()
    {
        Producer::ProductorMap::iterator iter = Producer::m_mapProducer.find( "PRODUCER" );
        if( iter == Producer::m_mapProducer.end() )
        {
            ErrorMan::setError( ErrorMan::eMisc, "can not find main producer:PRODUCER" );
            return false;
        }

        list< Producer* > dependStack;
        dependStack.push_back( &(iter->second) );

        list< Producer* > confusedList;
        while( !dependStack.empty() )
        {
            Producer* top = dependStack.back();
            confusedList.clear();
            if( !top->confusedProductors( confusedList ) )
                return false;
            if( confusedList.size() )
            {
                list< Producer* >::iterator iter = confusedList.begin();
                for ( ; iter != confusedList.end(); ++iter )
                {
                    if( find( dependStack.begin(), dependStack.end(), *iter) == dependStack.end() )
                        dependStack.push_back( *iter );
                }
            }else
            {
                dependStack.pop_back();
            }
            if( dependStack.size() && top == dependStack.back() )
            {
                string err = "producer" + top->name() + "can not be instantiation";
                ErrorMan::setError( ErrorMan::eMisc, err );
                return false;
            }
        }
        m_mainProductor = new ProducerReference( &(iter->second) );
        return true;
    }
private:
    enum Status{ eIdle, eBuffIsSet, eRulesIsValid };
    char*   m_rules;
    size_t  m_rulesBuffSize;
    size_t  m_rulesLength;
    Status  m_status;
    char*   m_rulesAnalysisIndex;
    size_t  m_lineCount;
    ProducerReference* m_mainProductor;
};

struct Argument{
    bool showHelp;
    bool creatDefaultRule;
    const char* ruleFile;
    const char* unkonwArg;
    Argument()
    {
        showHelp = false;
        creatDefaultRule = false;
        ruleFile = NULL;
        unkonwArg = NULL;
    }
};

Argument getArgument( int argc, const char* argv[] )
{
    Argument args;
    bool toGetFileName = false;
    for( int i = 1; i < argc && args.unkonwArg == NULL; ++i )
    {
        const char* str = argv[i];
        int len = strlen( str );
        if( len == 2 )
        {
            if( str[0] == '-' )
            {
                char c = str[1];
                if( c == 'h' )
                    args.showHelp = true;
                else if( c == 'l' )
                    args.creatDefaultRule = true;
                else if( c == 'f' )
                    toGetFileName = true;
                else
                    args.unkonwArg = str;
            }else if( !toGetFileName )
            {
                args.unkonwArg = str;
            }else
            {
                toGetFileName = false;
                args.ruleFile = str;
            }
        }
    }
    return args;
}

int main( int argc, const char* argv[] )
{
    Crunchx crunchx;

    Argument args = getArgument( argc, argv );

    if( args.unkonwArg != NULL )
    {
        printf( "ERROR:unknown argument:%s\n", args.unkonwArg );
        args.showHelp = true;
    }

    if( args.showHelp )
    {
        printf( HELP_TXT );
        return 0;
    }

    ErrorMan::ErrorCode err = ErrorMan::eOk;
    if( args.creatDefaultRule )
    {
        err = crunchx.setRules( DEFAULT_RULES, strlen( DEFAULT_RULES ) );
        if( err == ErrorMan::eOk )
            err = crunchx.saveRulesFile( DEFAULT_RULES_FILE_NAME );
        return -err;
    }

    if( args.ruleFile != NULL )
    {
        err = crunchx.openRulesFile( args.ruleFile );
        if( err != ErrorMan::eOk )
        {
            printf( "error:can not open file:%s\n", args.ruleFile );
            return -err;
        }
    }else
    {
        err = crunchx.openRulesFile( DEFAULT_RULES_FILE_NAME );
        if( err != ErrorMan::eOk )
        {
            crunchx.setRules( DEFAULT_RULES, strlen( DEFAULT_RULES ) );
            crunchx.saveRulesFile( DEFAULT_RULES_FILE_NAME );
        }
    }

    if ( !crunchx.analysis() )
    {
        printf("ERROR:%s", ErrorMan::errorMessage().c_str() );
        return ErrorMan::errorCode();
    }

    string str;
    while ( !crunchx.atEnd() )
    {
        if( crunchx.product( str ) )
        {
            printf( "%s\n", str.c_str() );
            crunchx.makeNextProduct();
        }else
        {
            printf("ERROR:%s", ErrorMan::errorMessage().c_str() );
            return ErrorMan::errorCode();
        }
    }
    return 0;
}
