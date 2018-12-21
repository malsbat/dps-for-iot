package main

import (
	"bytes"
	"crypto/elliptic"
	"crypto/rand"
	"dps"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

func pad(in []byte, n int) (out []byte) {
	out = make([]byte, n)
	low := len(out) - len(in)
	copy(out[low:], in)
	return
}

func main() {
	// Pre-shared keys for testing only. DO NOT USE THESE KEYS IN A REAL APPLICATION!
	networkKeyId := dps.KeyId{
		0x4c, 0xfc, 0x6b, 0x75, 0x0f, 0x80, 0x95, 0xb3, 0x6c, 0xb7, 0xc1, 0x2f, 0x65, 0x2d, 0x38, 0x26,
	}
	networkKey := dps.KeySymmetric{
		0x11, 0x21, 0xbb, 0xf4, 0x9f, 0x5e, 0xe5, 0x5a, 0x11, 0x86, 0x47, 0xe6, 0x3d, 0xc6, 0x59, 0xa4,
		0xc3, 0x1f, 0x16, 0x56, 0x7f, 0x1f, 0xb8, 0x4d, 0xe1, 0x09, 0x28, 0x26, 0xd5, 0xc0, 0xf1, 0x34,
	}
	keyId := []dps.KeyId{
		dps.KeyId{0xed, 0x54, 0x14, 0xa8, 0x5c, 0x4d, 0x4d, 0x15, 0xb6, 0x9f, 0x0e, 0x99, 0x8a, 0xb1, 0x71, 0xf2},
		dps.KeyId{0x53, 0x4d, 0x2a, 0x4b, 0x98, 0x76, 0x1f, 0x25, 0x6b, 0x78, 0x3c, 0xc2, 0xf8, 0x12, 0x90, 0xcc},
	}
	keyData := []dps.KeySymmetric{
		dps.KeySymmetric{0xf6, 0xeb, 0xcb, 0xa4, 0x25, 0xdb, 0x3b, 0x7e, 0x73, 0x03, 0xe6, 0x9c, 0x60, 0x35, 0xae, 0x11,
			0xae, 0x40, 0x0b, 0x84, 0xf0, 0x03, 0xcc, 0xf9, 0xce, 0x5c, 0x5f, 0xd0, 0xae, 0x51, 0x0a, 0xcc},
		dps.KeySymmetric{0x2a, 0x93, 0xff, 0x6d, 0x96, 0x7e, 0xb3, 0x20, 0x85, 0x80, 0x0e, 0x21, 0xb0, 0x7f, 0xa7, 0xbe,
			0x3f, 0x53, 0x68, 0x57, 0xf9, 0x3c, 0x7a, 0x41, 0x59, 0xab, 0x22, 0x2c, 0xf8, 0xcf, 0x08, 0x21},
	}
	ca := "-----BEGIN CERTIFICATE-----\r\n" +
		"MIICJjCCAYegAwIBAgIJAOtGcTaglPb0MAoGCCqGSM49BAMCMCoxCzAJBgNVBAYT\r\n" +
		"AlVTMQwwCgYDVQQKDANEUFMxDTALBgNVBAMMBHJvb3QwHhcNMTgwMzAxMTgxNDMy\r\n" +
		"WhcNMjgwMjI3MTgxNDMyWjAqMQswCQYDVQQGEwJVUzEMMAoGA1UECgwDRFBTMQ0w\r\n" +
		"CwYDVQQDDARyb290MIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQBLlFmM8e0WHRE\r\n" +
		"KF3XQBUihJJ2vQepy40aa3rRsEElQHxSu5GFOvV/FZrywrwAthiTwtF999uxgjgD\r\n" +
		"0nAHCWMJvVYARljnDm1+CpZFSTBeJsw0S7s4nA4s3bm07L3neSsMIADa+tUbIhMY\r\n" +
		"G5OWJ645pcMm4pc/Sv8yZoxffaJu6BUSPsejUzBRMB0GA1UdDgQWBBR15MMwK1i9\r\n" +
		"T9Ux9ZkP+W2eZ77RODAfBgNVHSMEGDAWgBR15MMwK1i9T9Ux9ZkP+W2eZ77RODAP\r\n" +
		"BgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMCA4GMADCBiAJCALVJ7AvWFEmn7EgS\r\n" +
		"XSd432PYQLLhwVlmyMiAkiv8A93pQeofJBbnZHjJOQH3tttBhmLIMZy/npjkPqUJ\r\n" +
		"riJlVcRKAkIBIhqssJD6XDlyV42a989vmuB52FGsBayiIkoJgzeoTZLLoGFtddpg\r\n" +
		"KNuru7XZOpdiszeXTDSPY7gmvYZGhLr58ng=\r\n" +
		"-----END CERTIFICATE-----\r\n"
	publisherId := "DPS Test Publisher"
	publisherCert := "-----BEGIN CERTIFICATE-----\r\n" +
		"MIIB2jCCATsCCQDtkL14u3NJRDAKBggqhkjOPQQDBDAqMQswCQYDVQQGEwJVUzEM\r\n" +
		"MAoGA1UECgwDRFBTMQ0wCwYDVQQDDARyb290MB4XDTE4MDMwMTE4MTQzMloXDTI4\r\n" +
		"MDIyNzE4MTQzMlowODELMAkGA1UEBhMCVVMxDDAKBgNVBAoMA0RQUzEbMBkGA1UE\r\n" +
		"AwwSRFBTIFRlc3QgUHVibGlzaGVyMIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQB\r\n" +
		"igbpvXYHms+7wTa1BcAf3PQF3/6R/J92HcbiPtPGVNlPYdpCnyYEF7DoNvgI/Iag\r\n" +
		"EqUjryMWoxwi+KghG1BwA2MAKhn/ta4TAXfASPr9gzYK5g+pKFnOXqc4sWut/o8D\r\n" +
		"se6LU2D3PsQBs5/kCkbjz1/sKQVbDJGT5eTHQvC5nxjToZcwCgYIKoZIzj0EAwQD\r\n" +
		"gYwAMIGIAkIBIEo4NfnSh60U4srn2iSR/u5VFHi4Yy3PjlKlkmRDo+ClPVHPOK7y\r\n" +
		"8/82J1qlTw5GSR0snR4R5663D2s3w2e9fIwCQgCp3K8Y7fTPdpwOy91clBr3OFHK\r\n" +
		"sMt3kjq1vrcbVzZy50hGyGxjUqZHUi87/KuhkcMKSqDC6U7jEiEpv/WNH/VrZQ==\r\n" +
		"-----END CERTIFICATE-----\r\n"
	subscriberPrivateKey := "-----BEGIN EC PRIVATE KEY-----\r\n" +
		"Proc-Type: 4,ENCRYPTED\r\n" +
		"DEK-Info: AES-256-CBC,65E2556079AC9649D58B8CC72AE4A43E\r\n" +
		"\r\n" +
		"qWEHBFDO16P65LBjQecIrcql5bWuUx2SO87Qgllm576xolusU+iTExRVENjtO3Nl\r\n" +
		"Vil2EqdMX2KHdv9p282lW1Drl069SesP69LiOo0sMYJefWJZRSnbRL7e7tDTXuUz\r\n" +
		"p038ythZg7Ho6UggO6cvy08JomqMuJtwpJ6RTTFAsQMsEqCF8m0e26EdxrFUpkrM\r\n" +
		"imwGuJ3hGzJKTZYaqK8i17LK+m4W0FzXETXp+qDyp9LBuZTqBISJ7MH+LOnY4neZ\r\n" +
		"a/F20EFCFwL47sfQlZMsOYHw140IS2+YOyzOD051Gbw=\r\n" +
		"-----END EC PRIVATE KEY-----\r\n"
	subscriberPassword := "DPS Test Subscriber"
	subscriberId := "DPS Test Subscriber"
	subscriberCert := "-----BEGIN CERTIFICATE-----\r\n" +
		"MIIB2jCCATwCCQDtkL14u3NJRTAKBggqhkjOPQQDBDAqMQswCQYDVQQGEwJVUzEM\r\n" +
		"MAoGA1UECgwDRFBTMQ0wCwYDVQQDDARyb290MB4XDTE4MDMwMTE4MTQzMloXDTI4\r\n" +
		"MDIyNzE4MTQzMlowOTELMAkGA1UEBhMCVVMxDDAKBgNVBAoMA0RQUzEcMBoGA1UE\r\n" +
		"AwwTRFBTIFRlc3QgU3Vic2NyaWJlcjCBmzAQBgcqhkjOPQIBBgUrgQQAIwOBhgAE\r\n" +
		"AdPlr3YCutvRP0agz6KRmVVY4HuzS5zmEaBzkTCSWFkhugDgwmMgszDCAD5maqe5\r\n" +
		"nAHammIc/MSw1UK+JFLFzSffAB48lbymUgTtE41sXWx82gc6vwvU25DqnNxHgS0L\r\n" +
		"K0bVQweaXa4toICC3SLZD0iRDI1jUqZPwDCkbpF9LyDDa181MAoGCCqGSM49BAME\r\n" +
		"A4GLADCBhwJBP7gFuL3dePSkYG4LoBg1atH6+2xfJWg51ZV8diRXWIgRlC5u3kCQ\r\n" +
		"R+AJhf+Slik1tMQePTB5OojwrRYjw40iEDoCQgE6rg0vAE2AZVLYfVsz01we+Rov\r\n" +
		"L8bFbjmY7xtqNCqRgCP7Nb/DLED8ahqo+uI7tPx5EqxDWj0FdxewZnbnBorBug==\r\n" +
		"-----END CERTIFICATE-----\r\n"

	onKeyAndId := func(request *dps.KeyStoreRequest) int {
		return dps.SetKeyAndId(request, dps.KeySymmetric(networkKey), networkKeyId)
	}
	onKey := func(request *dps.KeyStoreRequest, id dps.KeyId) int {
		for i := 0; i < len(keyId); i++ {
			if bytes.Compare(keyId[i], id) == 0 {
				return dps.SetKey(request, dps.KeySymmetric(keyData[i]))
			}
		}
		if bytes.Compare(networkKeyId, id) == 0 {
			return dps.SetKey(request, dps.KeySymmetric(networkKey))
		}
		if bytes.Compare([]byte(publisherId), id) == 0 {
			return dps.SetKey(request, dps.KeyCert{publisherCert, "", ""})
		}
		if bytes.Compare([]byte(subscriberId), id) == 0 {
			return dps.SetKey(request, dps.KeyCert{subscriberCert, subscriberPrivateKey, subscriberPassword})
		}
		return dps.ERR_MISSING
	}
	onEphemeralKey := func(request *dps.KeyStoreRequest, key dps.Key) int {
		switch key.(type) {
		case dps.KeySymmetric:
			k := make([]byte, 32)
			_, err := rand.Read(k)
			if err != nil {
				return dps.ERR_FAILURE
			}
			return dps.SetKey(request, dps.KeySymmetric(k))
		case dps.KeyEC:
			var curve elliptic.Curve
			keyEC, _ := key.(dps.KeyEC)
			if keyEC.Curve == dps.EC_CURVE_P384 {
				curve = elliptic.P384()
			} else if keyEC.Curve == dps.EC_CURVE_P521 {
				curve = elliptic.P521()
			}
			d, x, y, err := elliptic.GenerateKey(curve, rand.Reader)
			if err != nil {
				return dps.ERR_FAILURE
			}
			n := (curve.Params().BitSize + 7) / 8
			return dps.SetKey(request, dps.KeyEC{keyEC.Curve, pad(x.Bytes(), n), pad(y.Bytes(), n), pad(d, n)})
		default:
			return dps.ERR_MISSING
		}
	}
	onCA := func(request *dps.KeyStoreRequest) int {
		return dps.SetCA(request, ca)
	}

	dps.SetDebug(0)
	encryption := 1
	for i := 0; i < len(os.Args); i++ {
		if os.Args[i] == "-x" {
			i++
			encryption, _ = strconv.Atoi(os.Args[i])
		} else if os.Args[i] == "-d" {
			dps.SetDebug(1)
		}
	}

	var keyStore dps.KeyStore
	var nodeId []byte
	if encryption == 0 {
		keyStore = dps.CreateKeyStore(onKeyAndId, onKey, onEphemeralKey, nil)
		nodeId = nil
	} else if encryption == 1 {
		keyStore = dps.CreateKeyStore(onKeyAndId, onKey, onEphemeralKey, nil)
		nodeId = nil
	} else if encryption == 2 {
		keyStore = dps.CreateKeyStore(onKeyAndId, onKey, onEphemeralKey, onCA)
		nodeId = []byte(subscriberId)
	}

	node := dps.CreateNode("/", keyStore, nodeId)
	dps.StartNode(node, dps.MCAST_PUB_ENABLE_RECV, 0)
	fmt.Printf("Subscriber is listening on port %v\n", dps.GetPortNumber(node))

	sub := dps.CreateSubscription(node, []string{"a/b/c"})
	dps.Subscribe(sub, func(sub *dps.Subscription, pub *dps.Publication, payload []byte) {
		fmt.Printf("Pub %v(%v) matches:\n", dps.PublicationGetUUID(pub), dps.PublicationGetSequenceNum(pub))
		fmt.Printf("  pub %v\n", strings.Join(dps.PublicationGetTopics(pub), " | "))
		fmt.Printf("  sub %v\n", strings.Join(dps.SubscriptionGetTopics(sub), " | "))
		fmt.Printf("%v\n", string(payload))
		if dps.PublicationIsAckRequested(pub) {
			ackMsg := fmt.Sprintf("This is an ACK from %v", dps.GetPortNumber(dps.PublicationGetNode(pub)))
			fmt.Printf("Sending ack for pub UUID %v(%v)\n", dps.PublicationGetUUID(pub), dps.PublicationGetSequenceNum(pub))
			fmt.Printf("    %v\n", ackMsg)
			dps.AckPublication(pub, []byte(ackMsg))
		}
	})

	for {
		time.Sleep(1 * time.Second)
	}
}
