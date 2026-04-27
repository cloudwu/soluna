$HOST_TYPE_DECL$

$API_EXTERN$

struct soluna_api {
	int version;

$API_DECL$
};

struct soluna_api *
extlua_soluna_api() {
	static struct soluna_api api = {
		SOLUNA_EXT_API_VERSION,

$API_STRUCT$
	};
	return &api;
}
