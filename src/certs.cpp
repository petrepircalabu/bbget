#include "certs.hpp"
#include <spdlog/spdlog.h>
#include <wincrypt.h>

namespace bbget {
namespace certs {

#ifdef WIN32
void add_windows_root_certs(boost::asio::ssl::context& ctx)
{
	HCERTSTORE hStore = CertOpenSystemStore(0, "ROOT");
	if (hStore == NULL) {
		spdlog::error("CertOpenSystemStore failed");
		return;
	}

	X509_STORE*    store    = X509_STORE_new();
	PCCERT_CONTEXT pContext = NULL;
	while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
		X509* x509 = d2i_X509(NULL, (const unsigned char**)&pContext->pbCertEncoded,
		                      pContext->cbCertEncoded);
		if (x509 != NULL) {
			X509_STORE_add_cert(store, x509);
			X509_free(x509);
		}
	}

	CertFreeCertificateContext(pContext);
	CertCloseStore(hStore, 0);

	SSL_CTX_set_cert_store(ctx.native_handle(), store);
}
#endif

} // namespace certs
} // namespace bbget
