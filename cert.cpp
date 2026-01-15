/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define REQUIRESSL

#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>
#include <znc/User.h>

class CCertMod : public CModule {
  public:
    void Delete(const CString& line) {
        if (CFile::Delete(PemFile())) {
            PutModule(t_s("Pem file deleted"));
        } else {
            PutModule(t_s(
                "The pem file doesn't exist or there was a error deleting the "
                "pem file."));
        }
    }

    void Info(const CString& line) {
        bool bStrip = line.Token(1).Equals("strip");
        if (HasPemFile()) {
            PutModule(t_f("You have a certificate in {1}")(PemFile()));
            // Display certificate fingerprint
            CString sSHA1, sSHA256, sSHA512;
            if (GetCertFingerprints(sSHA1, sSHA256, sSHA512)) {
                if (bStrip) {
                    sSHA1.Replace(":", "");
                    sSHA256.Replace(":", "");
                    sSHA512.Replace(":", "");
                }
                PutModule(t_f("SHA-1 fingerprint:   {1}")(sSHA1));
                PutModule(t_f("SHA-256 fingerprint: {1}")(sSHA256));
                PutModule(t_f("SHA-512 fingerprint: {1}")(sSHA512));
            }
        } else {
            PutModule(t_s(
                "You do not have a certificate. Please use the web interface "
                "to add a certificate"));
            if (GetUser()->IsAdmin()) {
                PutModule(t_f("Alternatively you can place one at {1}")(
                    PemFile()));
            }
        }
    }
    bool GetCertFingerprints(CString& sSHA1, CString& sSHA256,
                             CString& sSHA512) const {
        CFile fPem(PemFile());
        if (!fPem.Open()) {
            return false;
        }

        CString sPemData;
        fPem.ReadFile(sPemData);
        fPem.Close();

        BIO* bio = BIO_new_mem_buf(sPemData.data(), sPemData.size());
        if (!bio) return false;

        X509* pCert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!pCert) return false;

        unsigned char sha1_buf[SHA_DIGEST_LENGTH];
        unsigned int sha1_len = SHA_DIGEST_LENGTH;
        if (X509_digest(pCert, EVP_sha1(), sha1_buf, &sha1_len)) {
            sSHA1 = CString(reinterpret_cast<const char*>(sha1_buf), sha1_len)
                        .Escape_n(CString::EASCII, CString::EHEXCOLON);
        }

        unsigned char sha256_buf[SHA256_DIGEST_LENGTH];
        unsigned int sha256_len = SHA256_DIGEST_LENGTH;
        if (X509_digest(pCert, EVP_sha256(), sha256_buf, &sha256_len)) {
            sSHA256 =
                CString(reinterpret_cast<const char*>(sha256_buf), sha256_len)
                    .Escape_n(CString::EASCII, CString::EHEXCOLON);
        }

        unsigned char sha512_buf[SHA512_DIGEST_LENGTH];
        unsigned int sha512_len = SHA512_DIGEST_LENGTH;
        if (X509_digest(pCert, EVP_sha512(), sha512_buf, &sha512_len)) {
            sSHA512 =
                CString(reinterpret_cast<const char*>(sha512_buf), sha512_len)
                    .Escape_n(CString::EASCII, CString::EHEXCOLON);
        }

        X509_free(pCert);
        return true;
    }

    MODCONSTRUCTOR(CCertMod) {
        AddHelpCommand();
        AddCommand("delete", "", t_d("Delete the current certificate"),
                   [=](const CString& sLine) { Delete(sLine); });
        AddCommand("info", t_d("[strip]"),
                   t_d("Show the current certificate. Use [strip] to remove "
                       "colons."),
                   [=](const CString& sLine) { Info(sLine); });
    }

    ~CCertMod() override {}

    CString PemFile() const { return GetSavePath() + "/user.pem"; }

    bool HasPemFile() {
        if (!CFile::Exists(PemFile())) {
            return false;
        }

        FILE* pFile = fopen(PemFile().c_str(), "r");
        if (!pFile) {
            PutModule(t_s("Can not read file."));
            return false;
        }

        // Certificate parsing
        X509* pCert = PEM_read_X509(pFile, nullptr, nullptr, nullptr);
        if (!pCert) {
            fclose(pFile);
            PutModule(t_s("Certificate section is not valid."));
            return false;
        }

        // Private key parsing
        rewind(pFile);
        EVP_PKEY* pKey = PEM_read_PrivateKey(pFile, nullptr, nullptr, nullptr);
        fclose(pFile);

        if (!pKey) {
            X509_free(pCert);
            PutModule(t_s("Private Key secion is not valid."));
            return false;
        }

        // Key-certificate cryptographic matching
        EVP_PKEY* pCertKey = X509_get_pubkey(pCert);

        // Fix -Wdeprecated-declarations warning.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        bool bKeyMatch = (pCertKey && EVP_PKEY_eq(pKey, pCertKey) == 1);
#else
        bool bKeyMatch = (pCertKey && EVP_PKEY_cmp(pKey, pCertKey) == 1);
#endif
        if (!bKeyMatch) {
            // Cleanup and return false
            if (pCertKey) EVP_PKEY_free(pCertKey);
            X509_free(pCert);
            EVP_PKEY_free(pKey);
            PutModule(t_s("Private Key does not match."));
            return false;
        }

        // Cleanup resources
        if (pCertKey) EVP_PKEY_free(pCertKey);
        X509_free(pCert);
        EVP_PKEY_free(pKey);

        return bKeyMatch;
    }

    EModRet OnIRCConnecting(CIRCSock* pIRCSock) override {
        if (HasPemFile()) {
            pIRCSock->SetPemLocation(PemFile());
        }

        return CONTINUE;
    }

    CString GetWebMenuTitle() override { return t_s("Certificate"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName == "index") {
            Tmpl["Cert"] = CString(HasPemFile());
            if (HasPemFile()) {
                CString sSHA1, sSHA256, sSHA512;
                if (GetCertFingerprints(sSHA1, sSHA256, sSHA512)) {
                    Tmpl["SHA1"] = sSHA1;
                    Tmpl["SHA256"] = sSHA256;
                    Tmpl["SHA512"] = sSHA512;
                    Tmpl["HasHashes"] = "true";
                }
            }
            return true;
        } else if (sPageName == "update") {
            CFile fPemFile(PemFile());

            if (fPemFile.Open(O_WRONLY | O_TRUNC | O_CREAT)) {
                fPemFile.Write(WebSock.GetParam("cert", true, ""));
                fPemFile.Close();
            }

            WebSock.Redirect(GetWebPath());
            return true;
        } else if (sPageName == "delete") {
            CFile::Delete(PemFile());
            WebSock.Redirect(GetWebPath());
            return true;
        }

        return false;
    }
};

template <>
void TModInfo<CCertMod>(CModInfo& Info) {
    Info.AddType(CModInfo::UserModule);
    Info.SetWikiPage("cert");
}

NETWORKMODULEDEFS(CCertMod, t_s("Use a ssl certificate to connect to a server"))
