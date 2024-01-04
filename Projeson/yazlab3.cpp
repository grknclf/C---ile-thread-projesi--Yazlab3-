#include <iostream>

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>

using namespace std;

mutex mtx;

// Problem1 Kodu
class Masa {
public:
    bool dolu;
    int musteriID;

    Masa() : dolu(false), musteriID(-1) {}
};

class Garson {
public:
    int garsonID;

    Garson(int id) : garsonID(id) {}

    void siparisAl(Masa& masa, int musteriID) {
        this_thread::sleep_for(chrono::milliseconds(500));
        lock_guard<mutex> lock(mtx);
        masa.dolu = true;
        masa.musteriID = musteriID;
        cout << "Garson " << garsonID << ", Masa " << musteriID << "'den siparis aldı." << endl;
    }
};

class Asci {
public:
    int asciID;

    Asci(int id) : asciID(id) {}

    void yemekHazirla(Masa& masa) {
        this_thread::sleep_for(chrono::milliseconds(700));
        lock_guard<mutex> lock(mtx);
        cout << "Asci " << asciID << ", Masa " << masa.musteriID << "'den siparisi hazirladi." << endl;
        masa.dolu = false;
    }
};

class Musteri {
public:
    int musteriID;
    chrono::steady_clock::time_point girisZamani;

    Musteri(int id) : musteriID(id), girisZamani(chrono::steady_clock::now()) {}

    void yemekYe() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        lock_guard<mutex> lock(mtx);
        cout << "Musteri " << musteriID << ", yemegini yedi ve odeme yapti." << endl;
        // Update musteriID
        musteriID = -1;  // or any other appropriate update
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
            if (masaBul()) {
                continue;
            }

            bool musteriEklendi = false;
            for (const auto& bekleyenMusteri : beklemeListesi) {
                if (bekleyenMusteri.musteriID == musteriler.size() + 1) {
                    musteriEklendi = true;
                    break;
                }
            }

            if (!musteriEklendi) {
                Musteri yeniMusteri(musteriler.size() + 1);
                yeniMusteri.girisZamani = chrono::steady_clock::now();
                beklemeListesi.push_back(yeniMusteri);
                cout << "Musteri " << yeniMusteri.musteriID << " bekleme listesine eklendi." << endl;
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
                it = beklemeListesi.erase(it);
            } else if (it->beklemeSuresiDolmus()) {
                cout << "Musteri " << it->musteriID << " bekleme süresini doldurduğu için ayriliyor." << endl;
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
        std::cout << random_number << " müşteri" << std::endl;
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

            vector<thread> garsonThreads;
            vector<thread> asciThreads;
            vector<thread> musteriThreads;

            for (int i = 0; i < masalar.size(); ++i) {
                if (masalar[i].dolu) {
                    garsonThreads.emplace_back([&](int i) {
                        garsonlar[i % garsonlar.size()].siparisAl(masalar[i], masalar[i].musteriID);
                    }, i);

                    asciThreads.emplace_back([&](int i) {
                        asci[i % asci.size()].yemekHazirla(masalar[i]);
                    }, i);

                    musteriThreads.emplace_back([&](int i) {
                        musteriler[i].yemekYe();
                    }, i);

                    masalar[i].dolu = false;
                    --toplamMusteriSayisi;
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

            cout << "-------------------------------------------------\n\n"<<"Bir sonraki adima gecmek icin Enter'a basin." << "\n\n-------------------------------------------------\n\n";
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
    int toplama;
    int toplamSure;
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

    int toplamKazanc = gelenMusteri * 1;

    for (int i = 1; i <= ilkgelenMusteriSayisi; ++i) {
        int masalarinMaliyeti = masaSayisi * 1;
        int garsonlarinMaliyeti = garsonSayisi * 1;
        int ascilarinMaliyeti = asciSayisi * 1;

        int toplamMaliyet = masalarinMaliyeti + garsonlarinMaliyeti + ascilarinMaliyeti;

        toplama = toplamKazanc - toplamMaliyet;

        if (i < ilkgelenMusteriSayisi) {
            toplamSure -= araMusteriSuresi;
        }
    }

    cout << "Toplam kazanc: " << toplama << " birim" << endl;
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

    return 0;
}
