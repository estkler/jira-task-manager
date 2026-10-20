/* Private C++ helper: reduce Jira's simple SVG class styles to presentation attributes.
   This is deliberately not a general CSS implementation. Unsupported syntax fails closed. */
#ifndef JTM_SVG_CSS_H
#define JTM_SVG_CSS_H
struct SvgCssProperty { wchar_t name[24],value[128]; };
struct SvgCssRule { wchar_t name[64];SvgCssProperty properties[8];unsigned count; };
struct SvgCssRules { SvgCssRule items[32];unsigned count; };
static void CssSpace(const wchar_t **at)
{while(**at==L' '||**at==L'\t'||**at==L'\r'||**at==L'\n')(*at)++;}
static bool CssIdentifier(const wchar_t **at,wchar_t *name,size_t capacity)
{
    size_t count=0;while((**at>=L'a'&&**at<=L'z')||(**at>=L'A'&&**at<=L'Z')||(**at>=L'0'&&**at<=L'9')||**at==L'_'||**at==L'-'){
        if(count+1>=capacity)return false;name[count++]=*(*at)++;
    }name[count]=0;return count!=0;
}
static bool CssValue(const wchar_t *name,const wchar_t *value)
{
    if(!wcscmp(name,L"fill")||!wcscmp(name,L"stroke")){
        if(*value==L'#'){size_t length=wcslen(value+1);if(length!=3&&length!=6&&length!=8)return false;for(const wchar_t *at=value+1;*at;at++)if(!((*at>=L'0'&&*at<=L'9')||(*at>=L'a'&&*at<=L'f')||(*at>=L'A'&&*at<=L'F')))return false;return true;}
        return !wcscmp(value,L"none")||!wcscmp(value,L"currentColor")||!wcscmp(value,L"transparent");
    }
    if(!wcscmp(name,L"opacity")||!wcscmp(name,L"fill-opacity")||!wcscmp(name,L"stroke-opacity")||!wcscmp(name,L"stroke-width")){
        wchar_t *end=NULL;double number=wcstod(value,&end);return end!=value&&!*end&&isfinite(number)&&number>=0&&number<=(!wcscmp(name,L"stroke-width")?256:1);
    }
    return false;
}
static bool CssParse(const wchar_t *text,SvgCssRules *rules)
{
    const wchar_t *at=text;CssSpace(&at);
    while(*at){
        if(*at++!=L'.'||rules->count>=32)return false;SvgCssRule rule={};if(!CssIdentifier(&at,rule.name,_countof(rule.name)))return false;
        CssSpace(&at);if(*at++!=L'{')return false;CssSpace(&at);
        while(*at&&*at!=L'}'){
            if(rule.count>=8)return false;SvgCssProperty *property=&rule.properties[rule.count];
            if(!CssIdentifier(&at,property->name,_countof(property->name)))return false;CssSpace(&at);if(*at++!=L':')return false;CssSpace(&at);
            const wchar_t *start=at;while(*at&&*at!=L';'&&*at!=L'}')at++;const wchar_t *end=at;while(end>start&&(end[-1]==L' '||end[-1]==L'\t'||end[-1]==L'\r'||end[-1]==L'\n'))end--;
            size_t count=(size_t)(end-start);if(!count||count>=_countof(property->value))return false;memcpy(property->value,start,count*sizeof(wchar_t));property->value[count]=0;
            if(!CssValue(property->name,property->value))return false;for(unsigned i=0;i<rule.count;i++)if(!wcscmp(rule.properties[i].name,property->name))return false;rule.count++;
            if(*at==L';')at++;CssSpace(&at);
        }
        if(!rule.count||*at++!=L'}')return false;
        /* Ambiguous duplicate selectors are outside the tiny supported subset. */
        for(unsigned i=0;i<rules->count;i++)if(!wcscmp(rules->items[i].name,rule.name))return false;rules->items[rules->count++]=rule;CssSpace(&at);
    }
    return true;
}
static HRESULT SvgXmlReader(IStream *stream,IXmlReader **reader)
{
    HRESULT hr=CreateXmlReader(__uuidof(IXmlReader),(void**)reader,NULL);
    if(SUCCEEDED(hr))hr=(*reader)->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit);
    if(SUCCEEDED(hr))hr=(*reader)->SetProperty(XmlReaderProperty_MaxElementDepth,32);
    if(SUCCEEDED(hr))hr=(*reader)->SetInput(stream);return hr;
}
static bool SvgStyles(IXmlReader *reader,SvgCssRules *rules)
{
    XmlNodeType type;HRESULT hr=S_OK;unsigned nodes=0;
    while((hr=reader->Read(&type))==S_OK){
        if(++nodes>2048)return false;if(type!=XmlNodeType_Element)continue;
        const wchar_t *name=NULL,*space=NULL;reader->GetLocalName(&name,NULL);reader->GetNamespaceUri(&space,NULL);
        if(wcscmp(name,L"style"))continue;UINT depth=0,attributes=0;reader->GetDepth(&depth);reader->GetAttributeCount(&attributes);
        if(wcscmp(space,L"http://www.w3.org/2000/svg")||depth!=1||attributes||reader->IsEmptyElement())return false;
        wchar_t css[8193]={0};size_t used=0;bool end=false;
        while((hr=reader->Read(&type))==S_OK){
            if(++nodes>2048)return false;if(type==XmlNodeType_EndElement){end=true;break;}
            if(type!=XmlNodeType_Text&&type!=XmlNodeType_Whitespace&&type!=XmlNodeType_CDATA)return false;
            const wchar_t *value=NULL;UINT length=0;if(FAILED(reader->GetValue(&value,&length))||used+length>8192)return false;memcpy(css+used,value,length*sizeof(wchar_t));used+=length;css[used]=0;
        }
        if(!end||!CssParse(css,rules))return false;
    }
    return hr==S_FALSE;
}
static IStream *NativeSvgNormalizedStream(const unsigned char *bytes,size_t length)
{
    if(!bytes||!length||length>128u*1024u)return NULL;IStream *input=SHCreateMemStream(bytes,(UINT)length);if(!input)return NULL;
    IXmlReader *reader=NULL;IXmlWriter *writer=NULL;IStream *output=NULL;SvgCssRules *rules=(SvgCssRules*)calloc(1,sizeof(SvgCssRules));
    HRESULT hr=rules?SvgXmlReader(input,&reader):E_OUTOFMEMORY;if(SUCCEEDED(hr)&&!SvgStyles(reader,rules))hr=E_INVALIDARG;
    if(reader){reader->Release();reader=NULL;}LARGE_INTEGER start={};input->Seek(start,STREAM_SEEK_SET,NULL);
    if(SUCCEEDED(hr))hr=SvgXmlReader(input,&reader);if(SUCCEEDED(hr))hr=CreateStreamOnHGlobal(NULL,TRUE,&output);
    if(SUCCEEDED(hr))hr=CreateXmlWriter(__uuidof(IXmlWriter),(void**)&writer,NULL);if(SUCCEEDED(hr))hr=writer->SetOutput(output);
    if(SUCCEEDED(hr))hr=writer->SetProperty(XmlWriterProperty_OmitXmlDeclaration,TRUE);
    XmlNodeType type;unsigned nodes=0;
    while(SUCCEEDED(hr)&&(hr=reader->Read(&type))==S_OK){
        if(++nodes>2048){hr=E_INVALIDARG;break;}
        if(type==XmlNodeType_Element){
            const wchar_t *name=NULL,*space=NULL;reader->GetLocalName(&name,NULL);reader->GetNamespaceUri(&space,NULL);
            if(wcscmp(space,L"http://www.w3.org/2000/svg")){hr=E_INVALIDARG;break;}
            if(!wcscmp(name,L"style")){do{hr=reader->Read(&type);}while(hr==S_OK&&type!=XmlNodeType_EndElement);if(hr!=S_OK)hr=E_INVALIDARG;continue;}
            bool empty=reader->IsEmptyElement()!=FALSE;hr=writer->WriteStartElement(NULL,name,space);if(FAILED(hr))break;
            const SvgCssRule *rule=NULL;
            if(reader->MoveToAttributeByName(L"class",NULL)==S_OK){const wchar_t *class_name=NULL;reader->GetValue(&class_name,NULL);for(unsigned i=0;i<rules->count;i++)if(!wcscmp(rules->items[i].name,class_name)){rule=&rules->items[i];break;}if(!rule){hr=E_INVALIDARG;break;}reader->MoveToElement();}
            HRESULT attribute=reader->MoveToFirstAttribute();
            while(SUCCEEDED(hr)&&attribute==S_OK){
                const wchar_t *value=NULL,*prefix=NULL;UINT value_length=0;reader->GetLocalName(&name,NULL);reader->GetNamespaceUri(&space,NULL);reader->GetPrefix(&prefix,NULL);reader->GetValue(&value,&value_length);
                if(value_length>32768){hr=E_INVALIDARG;break;}
                bool skip=!wcscmp(space,L"http://www.w3.org/2000/xmlns/")||(!*space&&!wcscmp(name,L"class"));
                if(rule&&!*space)for(unsigned i=0;i<rule->count;i++)if(!wcscmp(name,rule->properties[i].name))skip=true;
                if(!skip)hr=writer->WriteAttributeString(*prefix?prefix:NULL,name,*space?space:NULL,value);attribute=reader->MoveToNextAttribute();
            }
            if(FAILED(attribute))hr=attribute;reader->MoveToElement();
            if(rule)for(unsigned i=0;SUCCEEDED(hr)&&i<rule->count;i++)hr=writer->WriteAttributeString(NULL,rule->properties[i].name,NULL,rule->properties[i].value);
            if(SUCCEEDED(hr)&&empty)hr=writer->WriteEndElement();
        }else if(type==XmlNodeType_EndElement)hr=writer->WriteEndElement();
        else if(type==XmlNodeType_Text||type==XmlNodeType_Whitespace||type==XmlNodeType_CDATA){const wchar_t *value=NULL;reader->GetValue(&value,NULL);hr=type==XmlNodeType_Whitespace?writer->WriteWhitespace(value):writer->WriteString(value);}
        else if(type==XmlNodeType_ProcessingInstruction||type==XmlNodeType_DocumentType){hr=E_INVALIDARG;break;}
    }
    if(hr==S_FALSE)hr=writer->Flush();STATSTG stat={};if(SUCCEEDED(hr))hr=output->Stat(&stat,STATFLAG_NONAME);if(SUCCEEDED(hr)&&(!stat.cbSize.QuadPart||stat.cbSize.QuadPart>128u*1024u))hr=E_INVALIDARG;
    if(writer)writer->Release();if(reader)reader->Release();input->Release();free(rules);
    if(FAILED(hr)){if(output)output->Release();return NULL;}output->Seek(start,STREAM_SEEK_SET,NULL);return output;
}
#endif
