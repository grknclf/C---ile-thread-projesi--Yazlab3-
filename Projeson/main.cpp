#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <condition_variable>
#include <fstream>

using namespace std;

mutex mtx;
ofstream logDosyasi("log.txt");

class Masa {
public:
    bool dolu;
    int musteriID;
    unique_ptr<condition_variable> siparisAlindiCond;
    unique_ptr<mutex> siparisAlindiMtx;
    unique_ptr<condition_variable> yemekHazirCond;
    unique_ptr<mutex> yemekHazirMtx;

    Masa() : dolu(false), musteriID(-1),
             siparisAlindiCond(make_unique<condition_variable>()),
             siparisAlindiMtx(make_unique<mutex>()),
             yemekHazirCond(make_unique<condition_variable>()),
             yemekHazirMtx(make_unique<mutex>()) {}

    Masa(const Masa&) = delete;
    Masa& operator=(const Masa&) = delete;

    Masa(Masa&& other) = default;
    Masa& operator=(Masa&& other) = default;
};

class Garson {
public:
    int garsonID;
    static condition_variable siparisAlindi;
    static mutex siparisMtx;

    Garson(int id) : garsonID(id) {}

    void siparisAl(Masa& masa, int musteriID) {
        this_thread::sleep_for(chrono::seconds(2));

        {
            lock_guard<mutex> lock(siparisMtx);
            masa.dolu = true;
            masa.musteriID = musteriID;
            cout << "Garson " << garsonID << ", Masa " << musteriID << "'den siparis aldı." << endl;
            logDosyasi << "Garson " << garsonID << ", Masa " << musteriID << "'den siparis aldı." << endl;
        }
        siparisAlindi.notify_one();
    }
};

mutex Garson::siparisMtx;
condition_variable Garson::siparisAlindi;

class Asci {
public:
    int asciID;
    static condition_variable yemekHazir;
    static mutex yemekMtx;

    Asci(int id) : asciID(id) {}

    void yemekHazirla(Masa& masa) {
        unique_lock<mutex> lock(Garson::siparisMtx);
        Garson::siparisAlindi.wait(lock, [&masa] { return masa.dolu; });

        cout << "Asci " << asciID << ", Masa " << masa.musteriID << "'den siparisi hazirlamaya basladi." << endl;
        logDosyasi << "Asci " << asciID << ", Masa " << masa.musteriID << "'den siparisi hazirlamaya basladi." << endl;

        // Yemek hazırlama süreci 3 saniye sürüyor
        this_thread::sleep_for(chrono::seconds(3));

        {
            lock_guard<mutex> yemekLock(yemekMtx);
            cout << "Asci " << asciID << ", Masa " << masa.musteriID << "'un siparisi hazirladi." << endl;
            logDosyasi << "Asci " << asciID << ", Masa " << masa.musteriID << "'un siparisi hazirladi." << endl;
            masa.dolu = false;
        }
        yemekHazir.notify_one();
    }
};

mutex Asci::yemekMtx;
condition_variable Asci::yemekHazir;

class Musteri {
public:
    int musteriID;
    chrono::steady_clock::time_point girisZamani;

    Musteri(int id) : musteriID(id), girisZamani(chrono::steady_clock::now()) {}

    void yemekYe(Masa& masa) {
        unique_lock<mutex> lock(Asci::yemekMtx);
        Asci::yemekHazir.wait(lock, [&masa] { return !masa.dolu; });

        cout << "Masa " << musteriID << ", yemegini yemeye basladi." << endl;
        logDosyasi << "Masa " << musteriID << ", yemegini yemeye basladi." << endl;
        this_thread::sleep_for(chrono::seconds(3));  // Yemek yeme süresi
        cout << "Masa " << musteriID << ", yemegini yedi." << endl;
        logDosyasi << "Masa " << musteriID << ", yemegini yedi." << endl;

        this_thread::sleep_for(chrono::seconds(1));  // Ödeme süresi
        cout << "Masa " << musteriID << ", odeme yapti ve restorandan ayrildi." << endl;
        logDosyasi << "Masa " << musteriID << ", odeme yapti ve restorandan ayrildi." << endl;
    }

    bool beklemeSuresiDolmus() const {
        auto simdi = chrono::steady_clock::now();
        auto gecenSure = chrono::duration_cast<chrono::seconds>(simdi - girisZamani).count();
        return gecenSure >= 20;
    }
};
class Restoran {
public:
    vector<Masa> masalar;
    vector<Garson> garsonlar;
    vector<Asci> asci;
    vector<Musteri> musteriler;
    vector<Musteri> beklemeListesi;
    int bekleyenMusteriAyrilmaSuresi;

    Restoran(int masaSayisi, int garsonSayisi, int asciSayisi)
            : bekleyenMusteriAyrilmaSuresi(20)
    {
        for (int i = 0; i < masaSayisi; ++i) {
            masalar.push_back(Masa());
        }

        for (int i = 0; i < garsonSayisi; ++i) {
            garsonlar.push_back(Garson(i + 1));
        }

        for (int i = 0; i < asciSayisi; ++i) {
            asci.push_back(Asci(i + 1));
        }
    }

    void masalariDoldur(int musteriSayisi) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> randomMusteriSayisi(1, 20);

        int gelenMusteriSayisi = randomMusteriSayisi(gen);
        gelenMusteriSayisi = musteriSayisi;

        for (int i = 0; i < gelenMusteriSayisi; ++i) {
            lock_guard<mutex> lock(mtx);
            if (!masaBul()) {
                Musteri yeniMusteri(musteriler.size() + beklemeListesi.size() + 1);
                yeniMusteri.girisZamani = chrono::steady_clock::now();
                beklemeListesi.push_back(yeniMusteri);
                cout << "Musteri " << yeniMusteri.musteriID << " bekleme listesine eklendi." << endl;
                logDosyasi << "Musteri " << yeniMusteri.musteriID << " bekleme listesine eklendi." << endl;
            }
        }
    }


    bool masaBul() {
        for (int j = 0; j < masalar.size(); ++j) {
            if (!masalar[j].dolu) {
                masalar[j].dolu = true;
                masalar[j].musteriID = musteriler.size() + 1;
                musteriler.push_back(Musteri(masalar[j].musteriID));
                cout << "Musteri " << masalar[j].musteriID << " masa " << j + 1 << "'e oturdu." << endl;
                logDosyasi << "Musteri " << masalar[j].musteriID << " masa " << j + 1 << "'e oturdu." << endl;
                return true;
            }
        }
        return false;
    }

    void beklemeListesiniYonet() {
        lock_guard<mutex> lock(mtx);
        for (auto it = beklemeListesi.begin(); it != beklemeListesi.end();) {
            if (masaBul()) {
                cout << "Musteri " << it->musteriID << " bekleme listesinden masaya oturtuldu." << endl;
                logDosyasi << "Musteri " << it->musteriID << " bekleme listesinden masaya oturtuldu." << endl;
                it = beklemeListesi.erase(it);
            } else if (it->beklemeSuresiDolmus()) {
                cout << "Musteri " << it->musteriID << " bekleme süresini doldurduğu için ayriliyor." << endl;
                logDosyasi << "Musteri " << it->musteriID << " bekleme süresini doldurduğu için ayriliyor." << endl;
                it = beklemeListesi.erase(it);
            } else {
                ++it;
            }
        }
    }

    int randomMusteriSayisi () {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> uni(1, 10);

        int random_number = uni(rng);
        std::cout << random_number << " musteri" << std::endl;
        logDosyasi << random_number << " musteri" << std::endl;
        return (random_number);
    }

    void servisBaslat() {
        int adim = 1;
        int toplamMusteriSayisi = 0;

        while (adim <= 4) {
            cout << "\nAdim " << adim << ": ";
            toplamMusteriSayisi = randomMusteriSayisi();
            masalariDoldur(toplamMusteriSayisi);

            beklemeListesiniYonet();

            vector<thread> garsonThreads, asciThreads, musteriThreads;

            for (int i = 0; i < masalar.size(); ++i) {
                if (masalar[i].dolu) {
                    garsonThreads.emplace_back([this, i]() {
                        garsonlar[i % garsonlar.size()].siparisAl(masalar[i], masalar[i].musteriID);
                    });

                    asciThreads.emplace_back([this, i]() {
                        asci[i % asci.size()].yemekHazirla(masalar[i]);
                    });

                    musteriThreads.emplace_back([this, i]() {
                        musteriler[i].yemekYe(masalar[i]);
                    });
                }
            }

            for (auto& thread : garsonThreads) {
                thread.join();
            }

            for (auto& thread : asciThreads) {
                thread.join();
            }

            for (auto& thread : musteriThreads) {
                thread.join();
            }

            cout << "-------------------------------------------------\n\n"
                 << "Bir sonraki adima gecmek icin Enter'a basin."
                 << "\n\n-------------------------------------------------\n\n";
            if (adim == 1) {
                cin.get();
            }
            cin.get();
            adim++;
        }
    }
};

// Problem2 Kodu
void problem2() {
    // (Problem2 Kodu) ... (Buraya Problem2 kodunu ekle)
    int toplamKazanc;
    int toplamMaliyet;
    int toplamSure;
    int toplamMusteri;
    srand(std::time(0));
    int ayrilmisMusteri = rand() % 21;

    cout << "Toplam sureyi girin: ";
    cin >> toplamSure;

    int ilkgelenMusteriSayisi, araMusteriSuresi;

    cout << "Ilk gelen musteri sayisini girin: ";
    cin >> ilkgelenMusteriSayisi;

    cout << "Aralarindaki sureyi girin: ";
    cin >> araMusteriSuresi;

    int masaSayisi = ilkgelenMusteriSayisi;
    int garsonSayisi = ilkgelenMusteriSayisi / 2;
    int asciSayisi = garsonSayisi / 2;
    int gelenMusteri = ((toplamSure / araMusteriSuresi) * ilkgelenMusteriSayisi);
    toplamMusteri = gelenMusteri - ayrilmisMusteri;

    int Kazanc = toplamMusteri * 1;

    for (int i = 1; i <= ilkgelenMusteriSayisi; ++i) {
        int masalarinMaliyeti = masaSayisi * 1;
        int garsonlarinMaliyeti = garsonSayisi * 1;
        int ascilarinMaliyeti = asciSayisi * 1;
        toplamMaliyet = masalarinMaliyeti + garsonlarinMaliyeti + ascilarinMaliyeti;

        toplamKazanc = Kazanc - toplamMaliyet;

        if (i < ilkgelenMusteriSayisi) {
            toplamSure -= araMusteriSuresi;
        }
    }
    cout << "Restoranda toplam " << masaSayisi << " adet masa var" << endl;
    cout << "Restoranda toplam " << garsonSayisi << " adet garson var" << endl;
    cout << "Restoranda toplam " << asciSayisi << " adet asci var" << endl;
    cout << "Restorana  " << gelenMusteri << " adet musteri geldi" << endl;
    cout << "Ayrilan: " << ayrilmisMusteri << " musteri var" << endl;
    cout << "Restoranda hizmet alan  " << toplamMusteri << " musteri var" << endl;
    cout << "Bugunluk gelir: " << Kazanc << " birim" << endl;
    cout << "Bugunluk maliyet: " << toplamMaliyet << " birim" << endl;
    cout << "Toplam kazanc: " << toplamKazanc << " birim" << endl;
}

int main() {
    cout << "Hangi problemi cozmek istersiniz? (1: Problem1, 2: Problem2): ";
    int secim;
    cin >> secim;

    if (secim == 1) {
        int masaSayisi, garsonSayisi, asciSayisi;

        cout << "Masa sayisini girin: ";
        cin >> masaSayisi;

        cout << "Garson sayisini girin: ";
        cin >> garsonSayisi;

        cout << "Asci sayisini girin: ";
        cin >> asciSayisi;

        Restoran restoran(masaSayisi, garsonSayisi, asciSayisi);

        restoran.servisBaslat();
    } else if (secim == 2) {
        problem2();
    } else {
        cout << "Geçersiz seçim. Programdan çıkılıyor." << endl;
    }

    logDosyasi.close();

    return 0;
}
