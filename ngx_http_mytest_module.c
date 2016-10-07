#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>

static char * ngx_http_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r);

/*
 *ÿ��ngx_command_t�ṹ�嶨�����Լ�����Ȥ��һ��������
 */
static ngx_command_t  ngx_http_mytest_commands[] = {
	{								
		ngx_string("mytest"),		//name
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_NOARGS,	//type
		ngx_http_mytest,			//set ��������nameָ����������󽫻����set���������������Ĳ���
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
	},
	ngx_null_command
};

/*
 *������ǵ�ģ��û��ʲô�����ұ���Ҫ��HTTP��ܳ�ʼ���׶���ɵ�
 *�Ͳ���ʵ��ngx_http_module_t��8���ص�����
 */
static ngx_http_module_t  ngx_http_mytest_module_ctx = {
	NULL,	/* preconfiguration */
	NULL,	/* postconfiguration */
	NULL,	/* create main configuration */
	NULL,	/* init main configuration */
	NULL,	/* create server configuration */
	NULL,	/* merge server configuration */
	NULL,	/* create location configuration */
	NULL	/* merge location configuration */
};

ngx_module_t ngx_http_mytest_module = {
	NGX_MODULE_V1,
	&ngx_http_mytest_module_ctx,	//module context
	ngx_http_mytest_commands,		//module directives
	NGX_HTTP_MODULE,				//module type
	NULL,							//init master
	NULL,							//init module
	NULL,							//init process
	NULL,							//init thread
	NULL,							//exit thread
	NULL,							//exit process
	NULL,							//exit master
	NGX_MODULE_V1_PADDING
};

static char * ngx_http_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	    ngx_http_core_loc_conf_t  *clcf;
		clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);//
		/*������mytestģ���������������nginx������HTTP�����ͷ����Ϣ��
		 *�ͻ����HTTP�������������HTTP��ܹ���11���׶Σ���NGX_HTTP_CONTENT_PHASE
		 *�׶ν��п��ܵ���mytestģ������������Ҳ���ǵ���ngx_http_mytest_handler
		 *
		 *ngx_int_t (*ngx_http_handler_pt)(ngx_http_request_t *r)
		 *�������handler�ص�������ԭ�ͣ�����r��nginx��HTTP��ܽ������û��������
		 *�������ݽṹ�����Դ���ֱ�ӻ�ȡ������ص���Ϣ��
		 *����ֵ������ngx_http_request.h 72�п�ʼ�ĺ�,Ҳ������ngx_core.h 36�п�ʼ��ȫ�ִ�����
		 */
		 
		clcf->handler = ngx_http_mytest_handler;
		return NGX_CONF_OK;
}

static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r)
{
	/*ֻ֧��GET �� HEAD����*/
	if(!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD)))
		return NGX_HTTP_NOT_ALLOWED;
	/*���������еİ���*/
	ngx_int_t rc = ngx_http_discard_request_body(r);
	if(rc != NGX_OK)
		return rc;

	/*����HTTPͷ�����Լ�����*/
	ngx_str_t type = ngx_string("text/plain");
	//ngx_str_t response = ngx_string("Hello World!");
	r->headers_out.status = NGX_HTTP_OK;
	//r->headers_out.content_length_n = response.len;
	r->headers_out.content_type = type;
	/*֧���ļ��Ķϵ�����*/
	r->allow_ranges = 1;
	/*����ngx_buf_t׼�����Ͱ���,���buf�������ļ�*/
	ngx_buf_t *b = NULL;
	b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
	u_char* filename = (u_char*)"/tmp/test.txt";
	b->in_file = 1;
	b->file = ngx_palloc(r->pool, sizeof(ngx_file_t));
	b->file->fd = ngx_open_file(filename, NGX_FILE_RDONLY|NGX_FILE_NONBLOCK, NGX_FILE_OPEN, 0);
	b->file->log = r->connection->log;
	b->file->name.data = filename;
	b->file->name.len = strlen((const char *)filename);
	b->file_pos = 0;
	b->file_last = b->file->info.st_size;
	if(b->file->fd <= 0)
		return NGX_HTTP_NOT_FOUND;
	if(ngx_file_info(filename, &b->file->info) == NGX_FILE_ERROR)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;

	r->headers_out.content_length_n = b->file->info.st_size;
	b->last_buf = 1;
	/*���췢�Ͱ���ʱ��ngx_chain_t�ṹ��*/
	ngx_chain_t out;
	out.buf = b;
	out.next = NULL;
	
	/*����HTTPͷ��*/
	rc = ngx_http_send_header(r);



	/*��Ϊ��Ӧ����Ϣbody�Ļ��������д��ļ������������������ʱ��һ��
	 *Ҫ�����ļ��������������������ڴ�ص��еģ����ڴ�����������ڽ�����ʱ���
	 *��������Ҫ����Ķ���Ļص�����(������C++��������)����������Ҫ���ľ���ʵ����
	 *һ���ڴ���������������Ļص�����Ϊngx_pool_cleanup_file(ngxʵ�ֵ�ר����
	 *�������ڴ�����ļ��ĺ���)Ȼ����䴫�ݸ��ص������Ĳ�����
	 */
	/*һ��Ҫ���������cleaner�ͺͱ������junk����������Ĺ�ϵ*/
	/*���ڴ�����һ���������cleaner*/
	ngx_pool_cleanup_t *cleaner = ngx_pool_cleanup_add(r->pool, sizeof(ngx_pool_cleanup_file_t));
	/*����������Ļص�����Ϊר�������ļ���ngx_pool_cleanup_file����*/
	cleaner->handler = ngx_pool_cleanup_file;
	/*����������cleaner�Ļص������Ĳ�����Ҳ���ǵȴ�������Ķ���junk*/
	ngx_pool_cleanup_file_t *junk = cleaner->data;
	/*���ȴ�������Ķ���junk*/
	junk->fd = b->file->fd;
	junk->log = r->pool->log;
	junk->name = b->file->name.data;

#if 0

	/*����ngx_buf_t׼�����Ͱ���*/
	ngx_buf_t *body_buf = NULL;
	body_buf = ngx_create_temp_buf(r->pool, response.len);
	if(body_buf == NULL)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	/*���������ݿ�����body_buf��ȥ*/
	ngx_memcpy(body_buf->pos, response.data, response.len);
	/*pos �� last֮���ʾ�����������ݣ�����һ��Ҫ���ú�last�������Ƿ��Ͳ���ȥ��*/
	body_buf->last = body_buf->pos + response.len;
	/*��ʾ�������һ�黺����*/
	body_buf->last_buf = 1;

#endif


	


	/*���һ��Ϊ���Ͱ��壬 ���ͽ�����HTTP��ܻ����ngx_http_finalize_request��������*/
	return ngx_http_output_filter(r, &out);
}








