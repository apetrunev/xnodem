#ifndef MUMPS_H_
#define MUMPS_H_

#include <node.h>
#include <node_object_wrap.h>

using namespace v8;
using namespace node;

class Gtm: public ObjectWrap
{
public:
	Gtm();
	~Gtm();
	static void Init(Handle<Object>);
private:
	static Handle<Value> New(const Arguments&);
	static Handle<Value> close(const Arguments&);
	static Handle<Value> data(const Arguments&);
	static Handle<Value> function(const Arguments&);
	static Handle<Value> get(const Arguments&);
	static Handle<Value> global_directory(const Arguments&);
	static Handle<Value> increment(const Arguments&);
	static Handle<Value> kill(const Arguments&);
	static Handle<Value> lock(const Arguments&);
	static Handle<Value> merge(const Arguments&);
	static Handle<Value> open(const Arguments&);
	static Handle<Value> order(const Arguments&);
	static Handle<Value> previous(const Arguments&);
	static Handle<Value> set(const Arguments&);
	static Handle<Value> unlock(const Arguments&);
	static Handle<Value> version(const Arguments&);
	/* not implemented yet */
	static Handle<Value> update(const v8::Arguments&);
	static Handle<Value> previous_node(const v8::Arguments&);
	static Handle<Value> retrieve(const v8::Arguments&);
	static Handle<Value> next_node(const v8::Arguments&);
};
#endif /* MUMPS_H_ */
