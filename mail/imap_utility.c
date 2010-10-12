#include "imap_utility.h"

FeriteScript *output_script = NULL;
FeriteObject *output_closure = NULL;

char *curhst = NIL;             /* currently connected host */
char *curusr = NIL;             /* current login user  */
char *global_pwd = NULL;

int   debug_cmail_module = 1;

#define OUTPUT_DEBUG 1
#define OUTPUT_NORMAL 2
#define OUTPUT_WARNING 3
#define OUTPUT_ERROR 4

void output_printf( int type, char *format, ... ) {
	va_list arguments;
	
	va_start( arguments, format );
	if( output_closure ) {
		char *msg = NULL;
		FeriteBuffer *output_buffer = NULL;
		FeriteVariable **params = NULL;
		FeriteFunction *function = ferite_object_get_function( output_script, output_closure, "invoke" );
		FeriteVariable *return_value = NULL;
		
		output_buffer = ferite_buffer_new(output_script, 0);
		ferite_buffer_vprintf( output_script, output_buffer, format, &arguments );
		msg = ferite_buffer_get( output_script, output_buffer, NULL );
		
		params = ferite_create_parameter_list_from_data( output_script, "lc", type, msg, NULL );
		return_value = ferite_call_function( output_script, output_closure, NULL, function, params );
		
		if( return_value ) {
			ferite_variable_destroy( output_script, return_value );
		}
		ferite_delete_parameter_list( output_script, params );
		ferite_buffer_delete(output_script, output_buffer);
		ffree_ngc(msg);
	} else {
		vprintf(format, arguments);
		printf("\n");
	}
	va_end(arguments);
}

void set_error_string( FeriteScript *script,  FeriteObject *object, char* str ) {
	FeriteVariable *estr;
	estr = ferite_hash_get(script, object->variables->variables, "_errstr");
 	if( estr == NULL )
 		return;
	ferite_str_set( script, VAS(estr), str, 0,  FE_CHARSET_DEFAULT );
}


int create_address(FeriteScript *script, FeriteVariable *object, ADDRESS *addr) {
	FeriteVariable *v;
	if( addr->mailbox ) {
		v = fe_new_str( "mailbox", addr->mailbox, 0, FE_CHARSET_DEFAULT );
		if( v == NULL ) {
			//set_error_string( script, self,"Internal ferite error" );
			return -1;
		}
		ferite_object_set_var(script, VAO(object), "mailbox", v );
	}

	if( addr->host ) {
		v = fe_new_str("host", addr->host, 0, FE_CHARSET_DEFAULT );
		if( v == NULL ) {
			//set_error_string( script, self, "Internal error" );
			return -1;
		}
		ferite_object_set_var(script, VAO(object), "host", v );
	}

	if( addr->personal ) {
		v = fe_new_str("name", addr->personal, 0, FE_CHARSET_DEFAULT );
		if( v == NULL ) {
			//set_error_string( script, self, "Internal error" );
			return -1;
		}
		ferite_object_set_var(script, VAO(object), "name", v );
	}

	return 0;
}


FeriteVariable *create_address_list(FeriteScript *script, ADDRESS *root) {
    FeriteNamespaceBucket *nsb;
	FeriteVariable *address_list, *item_list, *item;
	ADDRESS *curr;
	int error;

	nsb = ferite_find_namespace( script, script->mainns, "Mail.AddressList", FENS_CLS );
	if( nsb == NULL )
		return NULL;
	address_list = ferite_build_object( script, (FeriteClass *)nsb->data );
	if( address_list == NULL )
		return NULL;

	//Get the array (member of address_list) and populate it with
	//the entries in the linked list current
	item_list = ferite_hash_get( script, VAO(address_list)->variables->variables, "list" );

	nsb = ferite_find_namespace( script, script->mainns, "Mail.Address", FENS_CLS );
	if( nsb == NULL ) {
		return NULL;
	}

	curr=root;
	while( curr ) {
		item =  ferite_build_object( script, (FeriteClass *)nsb->data );
		if( item == NULL )
			return NULL;
		error = create_address( script, item, curr);
		if(error) {
			return NULL;
		}
		ferite_uarray_add( script, VAUA(item_list), item, NULL , FE_ARRAY_ADD_AT_END);
		curr = curr->next;
	}
	return address_list;
}


FeriteVariable *create_ferite_header_object( FeriteScript *script, ENVELOPE *env )
{
    FeriteVariable *header = NULL, *v = NULL;
    FeriteNamespaceBucket *nsb = NULL;
    #define BUFSIZE 1025
    char buf[BUFSIZE];
    int i = 0;

    if( env == NULL)
      return NULL;

    ADDRESS *address_source[6] = { env->from, env->reply_to, env->cc, env->bcc, env->sender, env->to };
    char *address_target[6] = { "from", "reply_to" , "cc", "bcc", "sender" , "to" };
    int n_address_source = 6;

    char *source[4] = { env->subject, env->date, env->message_id, env->in_reply_to };
    char *target[4] = { "subject", "date", "ID", "in_reply_to" };
    int  n_source = 4;

    nsb = ferite_find_namespace( script, script->mainns, "Mail.MessageHeader", FENS_CLS );
    if( nsb == NULL)
	    return NULL;

    header = ferite_build_object( script, (FeriteClass *)nsb->data );
    if( header == NULL )
	    return NULL;


    //Parse each address structure in address_source[] and write the result to the
    //corresonding object member in address_target[]
    for(i=0; i < n_address_source; i++) {
	    v = create_address_list( script, address_source[i] );
	    if( v == NULL ) {
		    //set_error_string( script, self, "Couldn't create address list" );
		    return NULL;
	    }
	    ferite_object_set_var(script, VAO(header), address_target[i], v );
    }

    //Copy  each header-field in source[] to the
    //corresonding object member in address_target[]
    for(i=0; i< n_source; i++) {
	    if( source[i] ) {
		    v = fe_new_str(target[i], source[i] , 0, FE_CHARSET_DEFAULT );
		    if( v == NULL ) {
			    //set_error_string( script, self, "Couldn't create mail header" );
			    return NULL;
		    }
		    ferite_object_set_var(script, VAO(header), target[i], v );
		    if( debug_cmail_module && strcmp( target[i], "subject" ) == 0 ) {
				output_printf(OUTPUT_DEBUG,"module.mail: Processing email with subject: '%s'", source[i] );
		    }
	    }
    }
    return header;
}
int caseless_compare( char *str1, char *str2 )
{
    size_t i = 0;

	if( strlen(str1) != strlen(str2) )
		return 0;
		
	for( i = 0; i < strlen(str1); i++ ) {
		if( toupper(str1[i]) != toupper(str2[i]) )
			return 0;
	}
	return 1;
}
FeriteVariable *create_ferite_content_object( FeriteScript *script, MAILSTREAM *stream, BODY *body, int msgno, char *sec )
{
	if( body != NULL ) {
		FeriteVariable *object = NULL,*v = NULL;
		FeriteNamespaceBucket *nsb = NULL;
		char *object_name = NULL;

		object_name = ( body->type == TYPEMULTIPART ) ? "Mail.MessageMultiPart" : "Mail.MessagePart" ;

		nsb = ferite_find_namespace( script, script->mainns, object_name , FENS_CLS );
		if( nsb == NULL )
			return NULL;
		object = ferite_build_object( script, (FeriteClass *)nsb->data );
		if( object == NULL )
			return NULL;

		v  = fe_new_lng("type",body->type);
		ferite_object_set_var(script, VAO(object), "type", v);
		output_printf(OUTPUT_DEBUG,"module.mail: setting type %d", body->type);
		if( body->subtype ) {
			v = fe_new_str( "subtype", body->subtype, 0, FE_CHARSET_DEFAULT );
			ferite_object_set_var( script, VAO(object), "subtype", v );
		}

		if( body->type == TYPEMULTIPART ) {
			PART *part = NULL;
			int i = 0;
			char sec2[200];
			FeriteVariable *ret = NULL, *parts = NULL;

			parts = ferite_hash_get(script,VAO(object)->variables->variables,"parts");
			part = body->nested.part;
			while( part ) {
				i++;
				if( sec ) {
					snprintf(sec2,200,"%s.%d",sec,i);
				} else {
					snprintf(sec2,200,"%d",i);
				}
				ret = create_ferite_content_object( script, stream, &part->body, msgno, sec2 );
				ferite_uarray_add( script, VAUA(parts), ret , NULL, FE_ARRAY_ADD_AT_END );
				part = part->next;
			}
			v = fe_new_lng("nparts", i);
			ferite_object_set_var(script, VAO(object), "nparts", v );
		}
		else
		{
			long len = 0,len2 = 0;
			unsigned char *buf = NULL, *buf2 = NULL;
			SIZEDTEXT src, dest;
			FeriteVariable *v = NULL;
			PARAMETER *param = NULL;
			int i = 0;

			if( sec == NULL )
				sec = "1";
			buf = mail_fetchbody( stream, msgno, sec, &len );

			switch(body->encoding){
				case ENCQUOTEDPRINTABLE:
					if( debug_cmail_module )
						output_printf(OUTPUT_DEBUG,"module.mail: Decoding from encoded quotable");
					buf2 = rfc822_qprint(buf,len,&len2);
					break;
				case ENCBASE64: 
					if( debug_cmail_module )
						output_printf(OUTPUT_DEBUG,"module.mail: Decoding from base64");
					buf2=rfc822_base64(buf,len,&len2);
					break;
				default: 
					buf2=buf;
					len2=len;
			}

			if( debug_cmail_module ) {
				output_printf(OUTPUT_DEBUG,"module.mail: id: %s, description: %s", body->id, body->description);
				output_printf(OUTPUT_DEBUG,"module.mail: block type: %d.%s", body->type, body->subtype);
			}
			if( body->parameter ) /* Try and get the content type correctly */ {
				PARAMETER *ptr = body->parameter;
				while( ptr != NULL ) {
					if( caseless_compare( ptr->attribute, "charset" ) ) {
						if( debug_cmail_module )
							output_printf(OUTPUT_DEBUG,"module.mail: Found content type for block: %s", ptr->value);
						v = fe_new_str("charset", ptr->value, 0, FE_CHARSET_DEFAULT);
						ferite_object_set_var(script, VAO(object), "charset", v);
					}
					ptr = ptr->next;
				}
			}
			v = fe_new_str("content", buf2, len2, FE_CHARSET_DEFAULT );
			ferite_object_set_var(script, VAO(object), "content", v );

			v = fe_new_lng("encoding", body->encoding );
			ferite_object_set_var(script, VAO(object), "encoding", v );

			if( body->id ) {
				v = fe_new_str("ID", body->id, strlen(body->id), FE_CHARSET_DEFAULT);
				ferite_object_set_var(script, VAO(object), "ID", v);
			}
			
			if( body->disposition.type && strcasecmp(body->disposition.type, "attachment") == 0) {
				param = body->disposition.parameter;
				while(param){
					if( param->attribute && ((strcasecmp(param->attribute,"filename") == 0) ||
												(strcasecmp(param->attribute,"name") == 0) ||
												(strcasecmp(param->attribute,"name*") == 0) ||
												(strcasecmp(param->attribute,"filename*") == 0) )) {
						v = fe_new_str("filename", param->value, 0, FE_CHARSET_DEFAULT );
						ferite_object_set_var(script, VAO(object), "filename", v );
						break;
					}
					param=param->next;
				}
			} else {
				param = body->parameter;
				while(param){
					if( param->attribute && ((strcasecmp(param->attribute,"filename") == 0) || 
												(strcasecmp(param->attribute,"name") == 0))) {
						v = fe_new_str("filename", param->value, 0, FE_CHARSET_DEFAULT );
						ferite_object_set_var(script, VAO(object), "filename", v );
						break;
					}
					param = param->next;
				}
			}
		}
		return object;
	}
	return NULL;
}

FeriteVariable *create_ferite_mail_object( FeriteScript *script, FeriteVariable *header, FeriteVariable *content )
{
    FeriteVariable *mailobject;
    FeriteNamespaceBucket *nsb;

    nsb = ferite_find_namespace( script, script->mainns, "Mail.Message", FENS_CLS );
    if( nsb == NULL )
	    return NULL;
    mailobject = ferite_build_object( script, (FeriteClass *)nsb->data );
    if( mailobject == NULL )
	    return NULL;
	if( header )
	    ferite_object_set_var(script, VAO(mailobject), "header", header );
	if( content )
	    ferite_object_set_var(script, VAO(mailobject), "content", content );
    return mailobject;
}

BODY*  create_imap_content_leaf( FeriteScript *script, FeriteVariable *leaf ){
	BODY *body = NULL;
	FeriteVariable *v = NULL;

	body =  mail_newbody();

	// fixme NULL/0 ?
	v = ferite_hash_get(script,VAO(leaf)->variables->variables,"encoding");
	RETURN_IF_NULL(v);
	body->encoding=VAI(v);

	v = ferite_hash_get( script, VAO(leaf)->variables->variables, "content");
	RETURN_IF_NULL(v);

	//fixme memcopy? Encode?
	body->contents.text.data =strdup(VAS(v)->data);
	body->contents.text.size =strlen(VAS(v)->data); //fixme

	v = ferite_hash_get( script, VAO(leaf)->variables->variables, "type" );
	RETURN_IF_NULL(v);
	body->type=VAI(v);

	v = ferite_hash_get(script,VAO(leaf)->variables->variables,"subtype");
	RETURN_IF_NULL(v);
	body->subtype=cpystr( VAS(v)->data );

	v = ferite_hash_get(script,VAO(leaf)->variables->variables,"ID");
	RETURN_IF_NULL(v);
	body->id=cpystr( VAS(v)->data );

	v  = ferite_hash_get(script,VAO(leaf)->variables->variables,"filename");
	RETURN_IF_NULL(v);
	if( strlen(VAS(v)->data)) {
		PARAMETER *param = mail_newbody_parameter();
		param->attribute = cpystr("filename");
		param->value=cpystr( VAS(v)->data );

		body->disposition.parameter = param;
		body->disposition.type = cpystr("attachment");
	}

	v = ferite_hash_get( script, VAO(leaf)->variables->variables, "filepath" );
	RETURN_IF_NULL(v);
	if( strlen(VAS(v)->data) ) {
		off_t size;
		int fd;
		char *buf;
		struct stat statinfo;

		fd = open(VAS(v)->data,O_RDONLY);
		if( fd == -1 ) {
			char buffer[2048];
			sprintf(buffer, "Unable to create email content leaf, unable to open file: %s", VAS(v)->data);
			ERROR_IF_NULL(NULL, script, buffer);
		}
		size=lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		buf=malloc(size*sizeof(char)+1);
		if(buf){
			read(fd,buf,size);
			body->contents.text.data=buf;
			body->contents.text.size=size;
			close(fd);
		}
		else{
			ferite_error(script, 0, "Out of memory\n");
			close(fd);
			return NULL;
		}
	}

	v = ferite_hash_get(script,VAO(leaf)->variables->variables, "charset");
	RETURN_IF_NULL(v);
	if( strlen(VAS(v)->data) ) {
		PARAMETER *param = mail_newbody_parameter();
		if( body->parameter )
			param->next = body->parameter;
		body->parameter=param;

		body->parameter->attribute=cpystr("CHARSET");
		body->parameter->value=cpystr(VAS(v)->data);
	}

	return body;
}


BODY *create_imap_content_object(FeriteScript* script, FeriteVariable* fe_parent)
{
	FeriteVariable *parts_list,*fe_child,*v;
	BODY *root,*new_body;
	PART *new_part,*last_part =  NULL;
	int i=0;

	//Get array of child objects
        parts_list = ferite_hash_get( script, VAO(fe_parent)->variables->variables, "parts");
	//This is true if the top level is not a multipart
	if( parts_list == NULL )
		return( create_imap_content_leaf( script, fe_parent ));


	root = mail_newbody();
        root->type=TYPEMULTIPART;
	v = ferite_hash_get( script, VAO(fe_parent)->variables->variables, "subtype");
	RETURN_IF_NULL(v);
	if( VAS(v)->data )
		root->subtype = cpystr( VAS(v)->data );

        //Loop trough child array
        i=0;
	while( i < VAUA(parts_list)->size) {
		new_part = mail_newbody_part();
		fe_child = ferite_uarray_get_index( script, VAUA(parts_list) , i );
		RETURN_IF_NULL(fe_child);

		// Is it a multipart or a leaf?
		if( ferite_hash_get( script, VAO(fe_child)->variables->variables, "parts"))
			new_body = create_imap_content_object( script , fe_child );
		else
			new_body = create_imap_content_leaf( script, fe_child );
		RETURN_IF_NULL(new_body);

		new_part->body = *new_body; //Fixme, this sucks

		if( last_part == NULL )
			root->nested.part = last_part = new_part;
		else
			last_part = last_part->next = new_part;
		i++;
	}
	return root;
}

ADDRESS *create_imap_address( FeriteScript* script, FeriteVariable* fe_object){
	ADDRESS *root=NULL, *newaddr;
	FeriteVariable *fe_list,*v,*fe_addr;
	int i;

	RETURN_IF_NULL(fe_object && VAO(fe_object));

	fe_list = ferite_hash_get( script, VAO(fe_object)->variables->variables, "list" );
	RETURN_IF_NULL(fe_list);

	for( i=0; i<  VAUA(fe_list)->size; i++ ){
		fe_addr  = ferite_uarray_get_index( script, VAUA(fe_list) , i );
		RETURN_IF_NULL(fe_addr);
		if(root == NULL)
			root = newaddr = mail_newaddr();
		else
			newaddr = newaddr->next =  mail_newaddr();

		v =  ferite_hash_get( script, VAO(fe_addr)->variables->variables , "mailbox");
		RETURN_IF_NULL(v);
		if(VAS(v)->data)
			newaddr->mailbox = cpystr(VAS(v)->data);


		v =  ferite_hash_get( script, VAO(fe_addr)->variables->variables , "host");
		RETURN_IF_NULL(v);
		if(VAS(v)->data)
			newaddr->host = cpystr(VAS(v)->data);


		v =  ferite_hash_get( script, VAO(fe_addr)->variables->variables , "name");
		RETURN_IF_NULL(v);
		if(VAS(v)->data)
			newaddr->personal = cpystr(VAS(v)->data);
	}
	return root;
}

ENVELOPE *create_imap_envelope( FeriteScript *script, FeriteVariable *header ){

	ENVELOPE *env = mail_newenvelope();
	FeriteVariable *v;
	ADDRESS *a;
	int i;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "to" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find to address");
	a = create_imap_address( script, v );
	if(a)
		env->to = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "from" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find from address");
	a = create_imap_address( script, v );
	if(a)
		env->from = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "cc" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find cc address");
	a = create_imap_address( script, v );
	if(a)
		env->cc = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "bcc" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find bcc address");
	a = create_imap_address( script, v );
	if(a)
		env->bcc = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "from" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find create return path");
	a = create_imap_address( script, v );
	if(a)
		env->return_path = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "sender" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find sender");
	a = create_imap_address( script, v );
	if(a)
		env->sender = a;

	v = ferite_hash_get( script, VAO(header)->variables->variables, "subject" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find subject");
	env->subject = cpystr( VAS(v)->data );
	RETURN_IF_NULL( env->subject );

	v = ferite_hash_get( script, VAO(header)->variables->variables, "in_reply_to" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find in reply to header");
	env->in_reply_to = cpystr( VAS(v)->data );
	RETURN_IF_NULL( env->in_reply_to );


	v = ferite_hash_get( script, VAO(header)->variables->variables, "ID" );
	ERROR_IF_NULL(v, script, "Unable to create imap envelope - unable to find proposed message id");
	env->message_id = cpystr( VAS(v)->data );

	return env;
}



ENVELOPE *create_env_from_object( FeriteScript *script, FeriteVariable *header )
{
    return NULL;
}
BODY *create_body_from_object( FeriteScript *script, FeriteVariable *content )
{
    return NULL;
}


/* Interfaces to C-client */

void mm_searched (MAILSTREAM *stream,unsigned long number)
{
}


void mm_exists (MAILSTREAM *stream,unsigned long number)
{
}


void mm_expunged (MAILSTREAM *stream,unsigned long number)
{
}


void mm_flags (MAILSTREAM *stream,unsigned long number)
{
}


void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
	mm_log (string,errflg);
}


void mm_list (MAILSTREAM *stream,int delimiter,char *mailbox,long attributes)
{
	if (attributes & LATT_NOINFERIORS) output_printf( OUTPUT_NORMAL, "%c%s, no inferiors", mailbox);
	if (attributes & LATT_NOSELECT)    output_printf( OUTPUT_NORMAL, "%c%s, no select", mailbox);
	if (attributes & LATT_MARKED)      output_printf( OUTPUT_NORMAL, "%c%s, marked", mailbox);
	if (attributes & LATT_UNMARKED)    output_printf( OUTPUT_NORMAL, "%c%s, unmarked", mailbox);
}


void mm_lsub (MAILSTREAM *stream,int delimiter,char *mailbox,long attributes)
{
	if (attributes & LATT_NOINFERIORS) output_printf( OUTPUT_NORMAL, "%c%s, no inferiors", mailbox);
	if (attributes & LATT_NOSELECT)    output_printf( OUTPUT_NORMAL, "%c%s, no select", mailbox);
	if (attributes & LATT_MARKED)      output_printf( OUTPUT_NORMAL, "%c%s, marked", mailbox);
	if (attributes & LATT_UNMARKED)    output_printf( OUTPUT_NORMAL, "%c%s, unmarked", mailbox);
}


void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
	if (status->flags & SA_MESSAGES)    output_printf( OUTPUT_NORMAL, "Mailbox %s, %lu messages", mailbox, status->messages);
	if (status->flags & SA_RECENT)      output_printf( OUTPUT_NORMAL, "Mailbox %s, %lu recent", mailbox, status->recent);
	if (status->flags & SA_UNSEEN)      output_printf( OUTPUT_NORMAL, "Mailbox %s, %lu unseen", mailbox, status->unseen);
	if (status->flags & SA_UIDVALIDITY) output_printf( OUTPUT_NORMAL, "Mailbox %s, %lu UID validity", mailbox, status->uidvalidity);
	if (status->flags & SA_UIDNEXT)     output_printf( OUTPUT_NORMAL, "Mailbox %s, %lu next UID", mailbox, status->uidnext);
}


void mm_log (char *string,long errflg)
{
	switch ((short) errflg) {
	case NIL:
		output_printf(OUTPUT_NORMAL, "%s", string);
		break;
	case PARSE:
	case WARN:
		output_printf(OUTPUT_WARNING, "%s", string);
		break;
	case ERROR:
		output_printf(OUTPUT_ERROR, "%s", string);
		break;
	default:
		output_printf(OUTPUT_NORMAL, "%s", string);
	}
}


void mm_dlog (char *string)
{
	output_printf(OUTPUT_DEBUG,"debug-log: %s", string);
}


void mm_login (NETMBX *mb,char *user,char *pwd,long trial)
{
	if (curhst)
		fs_give ((void **) &curhst);

	curhst = (char *)fs_get (1+strlen (mb->host));
	strcpy (curhst,mb->host);

	if (*mb->user)
		strcpy (user,mb->user);

	if (curusr)
		fs_give ((void **) &curusr);


	curusr = cpystr (user);
	strcpy(pwd,global_pwd);
}

void mm_critical (MAILSTREAM *stream)
{
}

void mm_nocritical (MAILSTREAM *stream)
{
}

long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
	kill (getpid (),SIGSTOP);
	return NIL;
}

void mm_fatal (char *string)
{
	output_printf(OUTPUT_ERROR,"Fatal error: %s", string);
}
