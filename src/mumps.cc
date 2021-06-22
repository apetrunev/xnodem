extern "C" {	
#include <sys/types.h>
	
#include <stdlib.h>	
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <iconv.h>        
#include <gtmxc_types.h>
}

#include "common.h"
#include "mumps.h"
#include "iconvm.h"

using namespace v8;
using namespace node;

int gtm_is_open;

static gtm_char_t retbuf[BUF_LEN_MAX];
static gtm_char_t databuf[BUF_LEN_MAX];
static gtm_char_t retconv[4*BUF_LEN_MAX];
static gtm_char_t errbuf[BUF_LEN];

static struct termios tp;

#define MODE_CANONICAL	0
#define MODE_STRICT	1

static gtm_int_t mode = MODE_CANONICAL;
static gtm_int_t auto_relink = 0;

#define setOk(obj, flag) \
	(obj)->Set(String::New("ok"), Number::New(flag))

#define setErrorCode(obj, code) \
	(obj)->Set(String::New("errorCode"), Number::New(code))

#define setErrorMessage(obj, errmsg) \
	(obj)->Set(String::New("errorMessage"), String::New(errmsg))
	
#define setResult(obj, result) \
	(obj)->Set(String::New("result"), result)

enum class M {
	M_DATA,
	M_FUNCTION,
	M_GET,
	M_GLOBAL_DIRECTORY,
	M_INCREMENT,
	M_KILL,
	M_LOCK,
	M_MERGE,
	M_NEXT_NODE,
	M_ORDER,
	M_PREVIOUS,
	M_PREVIOUS_NODE,
	M_RETRIEVE,
	M_SET,
	M_UNLOCK,
	M_UPDATE,
	M_VERSION
};

static void gtm_error_parse(gtm_char_t *err_str, int *err_code, gtm_char_t **err_msg)
{
	gtm_char_t *code;
	/* parse gtm error string */
	code = strtok_r(err_str, ",", err_msg);
	*err_code = atoi(code);
}

Handle<Value> Gtm::open(const Arguments &args)
{
	HandleScope scope;
	Local<Object> res = Object::New();
	gtm_status_t err;
	char *arelink;

	if (gtm_is_open) {
		setOk(res, 0);
		setErrorMessage(res, "gtm is opened already");
		return scope.Close(res);	
	}
	(void)tcgetattr(STDIN_FILENO, &tp); 
	/* init gtm runtime */
	err = gtm_init();
	if (err) {
		gtm_char_t *err_msg;
		int err_code;
		/* read error message from gtm */
  		gtm_zstatus(errbuf, sizeof(errbuf));
		gtm_error_parse(errbuf, &err_code, &err_msg);
		setOk(res, 0);
		setErrorCode(res, err_code);
		setErrorMessage(res, err_msg);
        	return scope.Close(res);
	}
	if ((arelink = getenv("XNODEM_AUTO_RELINK")) != NULL) {
		auto_relink = atoi(arelink);	
	}
	/* success */
	gtm_is_open = TRUE;
	setOk(res, 1);
	setResult(res, Number::New(1));
	return scope.Close(res);
}

Handle<Value> Gtm::close(const Arguments &args)
{
	HandleScope scope;
	Local<Object> res = Object::New();
	gtm_status_t err;
	/* nothing to close */
 	if (!gtm_is_open) {
		setOk(res, 0);
		setErrorMessage(res, "gtm is closed already");
		return scope.Close(res);
	}
 	err = gtm_exit();
	if (err) { 
		gtm_char_t *err_msg;
		int err_code;
		/* read error message from gtm */
  		gtm_zstatus(errbuf, sizeof(errbuf));
		gtm_error_parse(errbuf, &err_code, &err_msg);
		setOk(res, 0);
		setErrorCode(res, err_code);
		setErrorMessage(res, err_msg);
        	return scope.Close(res);
	}
	(void)tcsetattr(STDIN_FILENO, TCSANOW, &tp);
	/* successfuly closed */
	gtm_is_open = FALSE;
	setOk(res, 1);
	setResult(res, Number::New(1));
        return scope.Close(res);
}

Handle<Value> Gtm::version(const Arguments &args)
{
	HandleScope scope;
	Local<Object> res = Object::New();
	ci_name_descriptor call;
	gtm_status_t err;

	if (!gtm_is_open)
        	return scope.Close(String::New("Node.js Adaptor for GT.M"));

	call.rtn_name.address = "version";
	call.rtn_name.length = strlen(call.rtn_name.address);
	call.handle = NULL;

	err = gtm_cip(&call, retbuf, NULL);
	if (err) { 
		gtm_char_t *err_msg;
		int err_code;
		/* read error message from gtm */
  		gtm_zstatus(errbuf, sizeof(errbuf));
		gtm_error_parse(errbuf, &err_code, &err_msg);
		setOk(res, 0);
		setErrorCode(res, err_code);
		setErrorMessage(res, err_msg);
        	return scope.Close(res);
	}
	return scope.Close(String::New(retbuf));
}

/* parse json string to object with JSON.parse()*/
static Handle<Value> JSON_parse(Local<Value> json)
{
	HandleScope scope;
	/* organize global execution sandbox */
	Local<Context> context = Context::GetCurrent();
	Local<Object> global = context->Global();
	/* we will need JSON.parse() function
	 * so firstly we get `JSON' object
	 */
    	Local<Object> JSON = global->Get(String::New("JSON"))->ToObject();
	/* then we get `parse' method from the object */
    	Local<Function> JSONparse = Local<Function>::Cast(JSON->Get(String::New("parse")));
	/* and finally call JSON.parse with `json' */
    	return scope.Close(JSONparse->Call(JSON, 1, &json));
}

static void subs2mumps_array(Local<Array> &js_array, Local<Array> &mumps_array)
{
        HandleScope scope;
	gtm_char_t buf[1024];
	 
        for (unsigned int i = 0; i < js_array->Length(); i++) { 
                Local<String> item = Local<String>::Cast(js_array->Get(i)->ToString()); 
                size_t len = item->Length(); 
                if (len > SUBSCRIPT_LEN_MAX) 
                        ThrowException(Exception::Error(String::New("subscript is too big"))); 
		/* add space for quotation marks */ 
		sprintf(buf, "%zu:", len + 2); 
		Local<String> s = String::Concat(String::New("\""), String::Concat(item, String::New("\""))); 
		Local<String> tmp = String::Concat(String::New(buf), s); 
		mumps_array->Set(i, tmp); 
        }       
}

#define js2mumps_array(js_arr, m_arr) subs2mumps_array(js_arr, m_arr)

/* this function makes a string of form 'len1:"arg1",...,"lenN:"argN"
 * from array of argumts and put the result into global buffer `databuf`
 */
static Handle<Value> args2mumps_string(Local<Array> &js_array)
{
	HandleScope scope;
	gtm_char_t num[1024];
	size_t len = 0, n = 0, written_len = 0;
	char *encoding;
	Local<Object> err_obj = Object::New();
	
	for (unsigned int i = 0; i < js_array->Length(); i++) {
		Local<String> str = Local<String>::Cast(js_array->Get(i)->ToString());
		if ((encoding = getenv("XNODEM_ENCODING")) != NULL) {	
				iconv_t cd = iconvm_open(encoding, "utf8");
				if (cd == (iconv_t)(-1)) {
					setOk(err_obj, 0);
					setErrorMessage(err_obj, strerror(errno));
					return scope.Close(err_obj);
				}	
                               	/* left space for quotes and null byte */
                               	len = iconvm(cd, *String::Utf8Value(str), str->Utf8Length(), retbuf, sizeof(retbuf) - 3);
                               	if (iconvm_close(cd) < 0) {
                              		setOk(err_obj, 0);
                               	        setErrorMessage(err_obj, strerror(errno));
                               	        return scope.Close(err_obj);
                      		}
                              	/* make string of the form `size:"string",` */	
				n = snprintf(num, sizeof(num), "%zu:", len + 2);
				/* check available space in the buffer
				 * add one byte for comma
				 */
				if (n + len + 1 > sizeof(databuf) - written_len)
					ThrowException(Exception::Error(String::New("string exceed maximum length")));
				/* wrap in quoutes and appends comma */
				written_len += snprintf(databuf + written_len, sizeof(databuf) - written_len,
							"%zu:\"%s\",", len + 2, retbuf);		
		} else {
			/* size check */
			len = str->Length();
			n = snprintf(num, sizeof(num), "%zu:", len + 2);
			if (n + len + 1 > sizeof(databuf) - written_len)
				ThrowException(Exception::Error(String::New("string exceed maximum length")));	
			/* wrap in quoutes */
			written_len += snprintf(databuf + written_len, sizeof(databuf),
						"%zu:\"%s\",", len + 2, *String::Utf8Value(str));
		}
	}
	/* delete last comma */	
	if (databuf[written_len - 1] != '\0')
		databuf[written_len - 1] = '\0';
	
	return scope.Close(True());
}

static char* to_string(M type)
{
	switch (type) {
	case M::M_DATA:
		return "data";
	case M::M_FUNCTION:
		return "function";
	case M::M_GET:
		return "get";	
	case M::M_GLOBAL_DIRECTORY:
		return "global_directory";
	case M::M_INCREMENT:
		return "increment";
	case M::M_KILL:
		return "kill";
	case M::M_LOCK:
		return "lock";
	case M::M_MERGE:
		return "merge";
	case M::M_NEXT_NODE:
		return "next_node";
	case M::M_ORDER:
		return "order";
	case M::M_PREVIOUS:
		return "previous";
	case M::M_PREVIOUS_NODE:
		return "previous_node";
	case M::M_RETRIEVE:
		return "retrieve";
	case M::M_SET:
		return "set";
	case M::M_UNLOCK:
		return "unlock";
	case M::M_UPDATE:
		return "update";
	case M::M_VERSION:
		return "version";
	default:
		assert(FALSE);
	}
	return NULL;
}

Handle<Value> gtm_call(M function, const Arguments &_args)
{
	HandleScope scope;
	Local<Object> err_obj = Object::New();
	Local<Value> glb;
	Local<Value> subs;
	Local<Value> data;
	Local<Value> func;
	Local<Value> func_args;
	gtm_status_t err;
	ci_name_descriptor call;
	gtm_char_t *err_msg;
	int err_code;
	char *encoding;

	if (!gtm_is_open) {
		setOk(err_obj, 0);
		setErrorMessage(err_obj, "Gtm is closed");
		return scope.Close(err_obj);
	}
	
	Local<Object> args = Local<Object>::Cast(_args[0]);
	if (args->IsUndefined())
		return scope.Close(Undefined());

#define set_mumps_call(call, cname) \
do { \
	call.rtn_name.address = cname; \
	call.rtn_name.length  = strlen(call.rtn_name.address); \
	call.handle = NULL; \
} while (0);

#define throw_exception(message) \
do { \
	ThrowException(Exception::Error(String::New(message))); \
} while (0);

	switch (function) {
	case M::M_DATA:
	case M::M_KILL:
	case M::M_LOCK:
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));
	
			Local<Value> m_subs;
			Local<Array> js_subs;

			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs  = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);
				js2mumps_array(js_subs, tmp);
			}
			
			set_mumps_call(call, to_string(function));
	
			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs), mode);
	
			if (err)
				goto gtm_err;
	
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
			/* stringify data property */
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			/* set subs in response */
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined())
				ret_obj->Set(String::New("subscripts"), js_subs);
			return scope.Close(ret_obj);	
		}
		break;
	case M::M_FUNCTION:
		{
			func = args->Get(String::New("function"));
			func_args = args->Get(String::New("arguments"));

			if (func->IsUndefined())
				throw_exception("Need to supply a function property");
			
			Local<Value> m_func_args;

			if (func_args->IsUndefined()) {
				m_func_args = String::Empty();
			} else {
				Local<Array> js_func_args = Local<Array>::Cast(func_args);
				/* result is in the databuf */
                                args2mumps_string(js_func_args);
			}
	
			set_mumps_call(call, "function");

			/* pass data to mumps function */
			err = gtm_cip(&call, retbuf, *String::Utf8Value(func), databuf, auto_relink, mode);
			if (err)
				goto gtm_err;	
	
			Local<String> str;
			/* convert returned data back to utf8 */
			if ((encoding = getenv("XNODEM_ENCODING")) != NULL) {	
				iconv_t cd = iconvm_open("utf8", encoding);
				if (cd == (iconv_t)(-1)) {
					setOk(err_obj, 0);
					setErrorMessage(err_obj, strerror(errno));
					return scope.Close(err_obj);
				}
				iconvm(cd, retbuf, strlen(retbuf), retconv, sizeof(retconv));
				if (iconvm_close(cd) < 0) {
					setOk(err_obj, 0);
					setErrorMessage(err_obj, strerror(errno));
					return scope.Close(err_obj);
				}
				str = String::New(retconv);	
			} else {
				str = String::New(retbuf);			
			}

			if (str->Length() == 0) 
				throw_exception("No JSON string present");
	
			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
				
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Local<Array> args = Local<Array>::Cast(func_args);
			/* stringify data property */
			ret_obj->Set(String::New("arguments"), args);
			return scope.Close(ret_obj);
		}
		break;
	case M::M_GET:
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));
			
			Local<Value> m_subs;
			Local<Array> js_subs;

			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs  = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);
				js2mumps_array(js_subs, tmp);
			}
			
			set_mumps_call(call, "get");
	
			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs), mode);
			if (err)
				goto gtm_err;
		
			Local<String> str;

			if ((encoding = getenv("XNODEM_ENCODING")) != NULL) {
				iconv_t cd = iconvm_open("utf8", encoding);
				if (cd == (iconv_t)(-1)) {
					setOk(err_obj, 0);
					setErrorMessage(err_obj, strerror(errno));
					return scope.Close(err_obj);
				}
				iconvm(cd, retbuf, strlen(retbuf), retconv, sizeof(retconv));
				if (iconvm_close(cd) < 0) {
					setOk(err_obj, 0);
					setErrorMessage(err_obj, strerror(errno));
					return scope.Close(err_obj);
				}
				str = String::New(retconv);
			} else {
				str = String::New(retbuf);
			}
	
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
			/* stringify data property */
			ret_obj->Set(String::New("data"), String::New((char *)*String::Utf8Value(data_obj)));
			/* if no subs specified just return object */
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			/* set subs in response */
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined())
				ret_obj->Set(String::New("subscripts"), js_subs);
			return scope.Close(ret_obj);	
		}
		break;
	case M::M_GLOBAL_DIRECTORY:
		{
			Local<Value> max = args->Get(String::New("max"));
			Local<Value> lo  = args->Get(String::New("lo"));
			Local<Value> hi  = args->Get(String::New("hi"));
	
			if (max->IsUndefined())
				max = Number::New(0);
			if (lo->IsUndefined())
				lo = String::Empty();
			if (hi->IsUndefined())
				hi = String::Empty();
			
			set_mumps_call(call, "global_directory");
			
			err = gtm_cip(&call, retbuf, max->Uint32Value(),
						     *String::AsciiValue(lo),
						     *String::AsciiValue(hi));
			if (err)
				goto gtm_err;
	
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
			/* stringify data property */
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			return scope.Close(ret_obj);
		}
		break;
	case M::M_INCREMENT:
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));
			
			Local<Value> number = Local<Number>::Cast(_args[1]);
			if (number->IsUndefined())
				number = Number::New(1);
				
			Local<Value> m_subs;
			Local<Array> js_subs;
	
			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs  = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);	
				js2mumps_array(js_subs, tmp);
			}

			set_mumps_call(call, "increment");
	
			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs),
						      number->NumberValue(), mode);
	
			if (err)
				goto gtm_err;
			
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
			/* stringify data property */
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			/* if no subs specified just return object */
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			/* set subs in response */
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined())
				ret_obj->Set(String::New("subscripts"), js_subs);
			return scope.Close(ret_obj);
		}
		break;
	case M::M_UNLOCK:	
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));
	
			if (glb->IsUndefined())
				glb = String::Empty();

			Local<Value> m_subs;
			Local<Array> js_subs;

			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs  = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);	
				js2mumps_array(js_subs, tmp);
		 	}
	
			set_mumps_call(call, "unlock");

			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs), mode);
			if (err)
				goto gtm_err;

			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined())
				ret_obj->Set(String::New("subscripts"), js_subs);
			return scope.Close(ret_obj);	
		}
		break;
	case M::M_MERGE:
		{
			Local<Object> to_obj = Local<Object>::Cast(args->Get(String::New("to")));
			Local<Object> from_obj = Local<Object>::Cast(args->Get(String::New("from")));
			/* to */
			Local<Value> to_glb  = to_obj->Get(String::New("global"));
			Local<Value> to_subs = to_obj->Get(String::New("subscripts"));
			/* from */
			Local<Value> from_glb  = from_obj->Get(String::New("global"));
			Local<Value> from_subs = from_obj->Get(String::New("subscripts"));
	
			Local<Array> to_js_subs = Local<Array>::Cast(to_subs);
			Local<Array> to_m_subs  = Array::New();
			js2mumps_array(to_js_subs, to_m_subs);

			Local<Array> from_js_subs = Local<Array>::Cast(from_subs);
			Local<Array> from_m_subs  = Array::New();
			js2mumps_array(from_js_subs, from_m_subs);

			set_mumps_call(call, "merge");
	
			err = gtm_cip(&call, retbuf, *String::AsciiValue(to_glb),
						     *String::AsciiValue(to_m_subs),
						     *String::AsciiValue(from_glb),
						     *String::AsciiValue(from_m_subs), mode);
			if (err)
				goto gtm_err;
			
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");
		
			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
		
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));	
			return scope.Close(ret_obj);	
		}
		break;
	case M::M_ORDER:
	case M::M_PREVIOUS:
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));

			Local<Value> m_subs;
			Local<Array> js_subs;

			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);
				js2mumps_array(js_subs, tmp);
			}	
				
			set_mumps_call(call, to_string(function));
			
			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs), mode);
			if (err)
				goto gtm_err;
			
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");
		
			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
		
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined()) {
				js_subs->Set(Number::New(js_subs->Length() - 1), ret_obj->Get(String::New("result"))); 
				ret_obj->Set(String::New("subscripts"), js_subs);
			}
			return scope.Close(ret_obj);		
		}
		break;
	case M::M_SET:
		{
			glb  = args->Get(String::New("global"));
			subs = args->Get(String::New("subscripts"));
			data = args->Get(String::New("data"));
			
			if (data->IsUndefined()) 
				throw_exception("Need to supply a data property");
			
			if (args->Get(String::New("data"))->IsString()) {
				/* convert data from utf8 to `encoding' */
				if ((encoding = getenv("XNODEM_ENCODING")) != NULL) {	
					iconv_t cd = iconvm_open(encoding, "utf8");
					if (cd == (iconv_t)(-1)) {
						setOk(err_obj, 0);
						setErrorMessage(err_obj, strerror(errno));
						return scope.Close(err_obj);
					}
					
					Local<String> _data = Local<String>::Cast(data);
					/* left space for quotes and null byte */
					iconvm(cd, *String::Utf8Value(_data), _data->Utf8Length(), retbuf, sizeof(retbuf) - 3);
		
					if (iconvm_close(cd) < 0) {
						setOk(err_obj, 0);
						setErrorMessage(err_obj, strerror(errno));
						return scope.Close(err_obj);
					}
					/* wrap in quoutes */
					snprintf(databuf, sizeof(databuf), "\"%s\"", retbuf);
				} else {
					snprintf(databuf, sizeof(databuf), "\"%s\"", *String::Utf8Value(data));
				}
			}
		
			Local<Value> m_subs;
			Local<Array> js_subs;

			if (subs->IsUndefined()) {
				m_subs = String::Empty();
			} else {
				js_subs = Local<Array>::Cast(subs);
				m_subs = Array::New();
				Local<Array> tmp = Local<Array>::Cast(m_subs);
				js2mumps_array(js_subs, tmp);
			}

			set_mumps_call(call, "set");
			err = gtm_cip(&call, retbuf, *String::AsciiValue(glb),
						     *String::AsciiValue(m_subs),
						     databuf, mode);
			if (err)
				goto gtm_err;
		
			Local<String> str = String::New(retbuf);
			if (str->Length() == 0)
				throw_exception("No JSON string present");

			Handle<Value> ret = JSON_parse(str);
			if (ret.IsEmpty())
				return scope.Close(Undefined());
			
			Handle<Object> ret_obj = Handle<Object>::Cast(ret);
			Handle<Value> data_obj = ret_obj->Get(String::New("data"));
		
			ret_obj->Set(String::New("data"), String::New((char *)*String::AsciiValue(data_obj)));
			/* if no subs specified just return object */
			if (subs->IsUndefined())
				return scope.Close(ret_obj);
			/* if everything is ok then add subs to object */
			if (ret_obj->Get(String::New("errorCode"))->IsUndefined())
				ret_obj->Set(String::New("subscripts"), js_subs);
			return scope.Close(ret_obj);
		}
		break;
	/* not implemented yet */
	case M::M_NEXT_NODE:
	case M::M_PREVIOUS_NODE:
	case M::M_RETRIEVE:
	case M::M_UPDATE:
	default:
		return scope.Close(Undefined());
	}
gtm_err:
	gtm_zstatus(errbuf, sizeof(errbuf));
	gtm_error_parse(errbuf, &err_code, &err_msg);
	setOk(err_obj, 0);
	setErrorCode(err_obj, err_code);
	setErrorMessage(err_obj, err_msg);
	return scope.Close(err_obj);	
}
 
Handle<Value> Gtm::set(const Arguments &args)
{
	return gtm_call(M::M_SET, args);
}

Handle<Value> Gtm::get(const Arguments &args)
{
	return gtm_call(M::M_GET, args);
}

Handle<Value> Gtm::data(const Arguments &args)
{
	return gtm_call(M::M_DATA, args);
}

Handle<Value> Gtm::function(const Arguments &args)
{
	return gtm_call(M::M_FUNCTION, args);
}

Handle<Value> Gtm::global_directory(const Arguments &args)
{
	return gtm_call(M::M_GLOBAL_DIRECTORY, args);
}

Handle<Value> Gtm::increment(const Arguments &args)
{
	return gtm_call(M::M_INCREMENT, args);
}

Handle<Value> Gtm::kill(const Arguments &args)
{
	return gtm_call(M::M_KILL, args);
}

Handle<Value> Gtm::lock(const Arguments &args)
{
	return gtm_call(M::M_LOCK, args);
}

Handle<Value> Gtm::merge(const Arguments &args)
{
	return gtm_call(M::M_MERGE, args);
}

Handle<Value> Gtm::next_node(const Arguments &args)
{
	return gtm_call(M::M_NEXT_NODE, args);
}

Handle<Value> Gtm::order(const Arguments &args)
{
	return gtm_call(M::M_ORDER, args);
}

Handle<Value> Gtm::previous(const Arguments &args)
{
	return gtm_call(M::M_PREVIOUS, args);
}

Handle<Value> Gtm::previous_node(const Arguments &args)
{
	return gtm_call(M::M_PREVIOUS_NODE, args);
}

Handle<Value> Gtm::retrieve(const Arguments &args)
{
	return gtm_call(M::M_RETRIEVE, args);
}

Handle<Value> Gtm::unlock(const Arguments &args)
{
	return gtm_call(M::M_UNLOCK, args);
}

Handle<Value> Gtm::update(const Arguments &args)
{
	return gtm_call(M::M_UPDATE, args);
}

Gtm::Gtm() {}
Gtm::~Gtm() {}

Handle<Value> Gtm::New(const Arguments& args)
{
	HandleScope scope;
	/* invoked as constructor: new Gtm() */
	Gtm* obj = new Gtm();
	obj->Wrap(args.This());
	return args.This();
}

void Gtm::Init(Handle<Object> target)
{
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("Gtm"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

#define SET_GTM_METHOD(tpl, name, func) \
        tpl->PrototypeTemplate()->Set(String::NewSymbol(name), \
        FunctionTemplate::New(func)->GetFunction());
	SET_GTM_METHOD(tpl, "close", close);
	SET_GTM_METHOD(tpl, "open", open);
	SET_GTM_METHOD(tpl, "data", data);
	SET_GTM_METHOD(tpl, "function", function);
	SET_GTM_METHOD(tpl, "get", get);
	SET_GTM_METHOD(tpl, "global_directory", global_directory);
	SET_GTM_METHOD(tpl, "increment", increment);
	SET_GTM_METHOD(tpl, "kill", kill);
	SET_GTM_METHOD(tpl, "lock", lock);
	SET_GTM_METHOD(tpl, "merge", merge);
	SET_GTM_METHOD(tpl, "next", order);
	SET_GTM_METHOD(tpl, "next_node", next_node);
	SET_GTM_METHOD(tpl, "order", order);
	SET_GTM_METHOD(tpl, "previous", previous);
	SET_GTM_METHOD(tpl, "previous_node", previous_node);
	SET_GTM_METHOD(tpl, "retrieve", retrieve);
	SET_GTM_METHOD(tpl, "set", set);
	SET_GTM_METHOD(tpl, "unlock", unlock);
	SET_GTM_METHOD(tpl, "update", update);
	SET_GTM_METHOD(tpl, "version", version);	
#undef SET_GTM_METHOD
	Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
	target->Set(String::NewSymbol("Gtm"), constructor);
} 

/* Entry point */
void initialize(Handle<Object> target)
{
    Gtm::Init(target);
} 

NODE_MODULE(mumps, initialize)
