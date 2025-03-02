#set page(
	paper: "a4", margin: (x: 2.5cm, y: 2.5cm),
	header: context {
		let sel = selector(heading).before(here());
		let lvl = counter(sel);
		let headings = query(sel);
		if headings.len() == 0 { return; }
		if (lvl.get().at(0) == 0) { return; }
		let max_lvl = 2;
		let lvls = (1, 2);

		lvl.display((..n) => n.pos().slice(0, calc.min(max_lvl, n.pos().len())).map(str).join("."));
		h(1em);
		lvls.map(i => {
			let h = headings.filter(j => j.level == i);
			if h.len() == 0 { return none; }
			h.last().body
		}).filter(i => i != none).join([ --- ]);
	},
	numbering: "1"
)
#set text(size: 12pt, lang: "cs")
#set par(justify: true, first-line-indent: 0.2em, spacing: 1.5em)
#set heading(numbering: "1.")

#let title = [Simulování života mikroorganismů v modelovém mikrosvětě]

#set document(title: title, author: "Filip Majer", keywords: ())
#show outline.entry.where(level: 1): it => { v(12pt, weak: true); strong(it); }

#align(center, [#text(20pt)[*#title*]\
	Filip Majer\
	Gymnázium Jana Keplera])

#align(center, [#link("https://github.com/nat-int/mikrosim")])

#align(center, [_dokumentace je nedokončená_])

#align(bottom, outline(indent: auto))
#pagebreak()

= Úvod

Život je plný komplexních systémů, ale pro jeho zkoumání je někdy užitečné mít zjednodušený model, který jsme schopni plně propočítat. Naše schopnosti počítání posunuly vpřed počítače,
kterým dnes už můžeme dávat i poměrně komplikované modely.

== Cíle

Cílem tohoto projektu je vytvořit grafickou aplikaci modelu mikroorganismů. Model by měl být takový, aby nebyl moc daleko od středoškolského modelu, jak mikroorganismy fungují,
ale zároveň byl dostatečně jednoduchý, aby podle něj šlo najednou simulovat více variant organismů i na běžně výkonných počítačích.

== Související práce

Existuje hodně modelů i jejich simulátorů zjednodušeného života. Velká část z nich funguje tak, že se nadefinuje několik typů buněk, kde má každá své specifické chování, ze kterých se
poskládá organismus. Ten se pak může replikovat do stejné nebo podobné buněčné konfigurace. Jedním takovým simulátorem je Biogenesis (#link("https://biogenesis.sourceforge.net/")).

Jiná velká skupina modelů simuluje organismy jako objekty, které podle definovaných pravidel interagují s okolím a podle vstupů provádí různé akce. Vstupy typicky zpracovávají s využitím neuronových sítí. Příkladem
pro tuto skupinu je The bibites (#link("https://thebibites.itch.io/the-bibites")).

Oba tyto přístupy tak modelují spíše mnohobuněčné organismy než jednobuněčné mikroorganismy.

O něco obecnější model má projekt ALIEN (#link("https://alien-project.org/")), který je založený na simulaci částic, které mohou tvořit vazby a předávat si signály. ALIEN se snaží
o jednoduchý model a z něj vyvstávající chování, a tak v něm život není tak blízko realitě, jako bychom chtěli. Kromě toho je napsaný v CUDA, takže vyžaduje počítač s grafickou
kartou od společnosti Nvidia.

= Model

Model je ve dvou dimenzích, aby bylo snadnější jej implementovat a přehledně vykreslovat.

== Prostředí

Mikroorganismy žijí v definovaném prostředí. Jako základ prostředí této simulace byla zvolena tekutina, protože i skutečné mikroorganismy často žijí ve vodě.
Standardní způsoby simulování tekutin jsou částicové, eulerovské (kontinuální) a kombinované. V tomto prezentovaném modelu byl použit částicový model z tohoto simulátoru:
#link("https://github.com/SebLague/Fluid-Sim/tree/Episode-01"), především proto, že daný model je snadno implementovatelný, zároveň je pro částicové simulace snažší zabránit tvorbě hmoty z ničeho.
Do prezentovaného modelu byl převzat bez korekcí shlukování částic při rychlých ztrátách tlaku, protože chování tekutiny neovlivní moc a jejich vynecháním se model zjednodušší.
Snadnost implementace tu je na úkor realismu, tekutina je zde svými vlastnostmi mezi plynem, kapalinou a gelem. Přesto je dostatečně blízko reálné tekutině.

Tento model ve zkratce funguje tak, že tekutinu rozdělí na částice, kde každá z nich představuje malý objem tekutiny. Částice se zrychlují takovým směrem, aby se hustota (a tím i tlak) blížil požadované hodnotě.
Mimo to se rychlosti předávají mezi blízkými částicemi pro efekt viskozity.

Kromě tekutiny jsou v typickém prostředí, kde jednobuněčné mikroorganismy žijí, různé látky, ze kterých získávají energii nebo stavební materiály. Aby se simulace vešla do paměti a zároveň lépe paralelizovala,
jsou zde látky modelovány zjednodušeně jako čtveřice podčástí, které jsou červené, zelené, modré a šedé. Látky jsou symetrické vzhledem k rotaci (když se podčásti protočí, tak je to stále stejná látka).
Lze je zobrazit jako čtyřcípou hvězdu s cípy obarvenými na jednu ze čtyř barev.

#figure(image("compound.png", width: 15%), caption: ["látka" složená ze zelené, šedé, modré a modré podčásti])

Chemické chování látek (tvorba vazeb a vazebná energie) je inspirováno chovním malé organické látky, která obsahuje jeden uhlík se čtyřmi skupinami. Červená podčást se vlastnostmi blíží karboxylové skupině,
zelená hydroxylové, modrá jako aminové skuině a šedá neutrálnímu vodíku. Ke každé látce je přiřazena energie (uložená v jejích vazbách), která je určena takto:

$ sum^("podčásti")_(i) E_z(i) + c(i) dot (f("vedle i na jedné straně") + f("vedle i na druhé straně") + phi("naproti i")) $

$E_z$ dává energii vazby mezi podčástí a středem látky. $f$ určuje sílu indukčního efektu mezi vedlejšími pozicemi, $phi$ mezi protějšími pozicemi. $c$ dává sílu vlivu indukčního efektu z ostatních podčástí na podčást.
Energie látek jsou tak rozmanité a jen občas shodné (ale jsou shodně pro překlopené látky) a můžeme získávat zajímavé výsledky při počítání s jejich rozdíly.

Každá částice (každá kapka tekutiny) v sobě může mít libovolné množství každé látky. V průběhu simulace látky difundují. Při rozptylování dané látky se koncentrace látky z každé částice
rovnoměrně rozmístí do všech částic v okruhu se stejným poloměrem, jaký je dosah jejich silových interakcí.

== Proteiny

Funkce buněk provádí proteiny. Ty se skládají z aminokyselin, které se zřetězí a složí do tvaru, který jim umožní pracovat. Skládání proteinů je na tuto simulaci moc složité, a tak je zde model opět hodně
zjednodušený. V tomto modelu jsou aminokyseliny definovány jako látky vybrané z látek, které obsahují červenou a modrou podčást:

#figure(image("blocks.png", width: 100%), caption: [látky vybrané jako proteinogení])

Ze seznamu těchto látek pak skládáme protein tak, že za červený konec předchozí látky přidáme další látku tak, aby navazovala modrým koncem (jako translace probíhá od N-terminu k C-terminu za tvorby peptidických vazeb).
Protože jsou látky pravoúhlé, skládají se do mřížky. Když existuje více možností, jak by se mohla další látka navázat, vybere se ta, která v řetězci červených a modrých podčástí pokračuje co nejdéle rovně.

#figure(image("protein.png", width: 40%), caption: [ukázka proteinu (aa19c6a80100e (více v #link(<genom>)[sekci genom])), žluté pole značí aktivní místo]) <obrazek_protein>

Ze struktury je určena stabilita proteinu, což je podíl proteinů, která se nerozloží během jednoho kroku simulace:

$ min(1 - 15% dot e^(-s_c), 99%), s_c = n/80 + n_e/4 $

kde $n$ je počet polí v mřížce, kde je více látek najednou (protein překrývá sám se sebou - kříží) a $n_e$ je počet míst, kde je více látek najednou a zároveň je vedle prázdného pole.
Když se řetězec proteinu překříží, znamená to, že je zabalenější, a tak je náročnější ho rozkládat, takže je stabilnější. Když je překryv vedle prázdného pole, tak musí řetězec postupovat stejnou cestou,
a tak se struktura zpevňuje, a tak je o to více stabilnější.

V proteinech se nachází aktivní místa, kde dochází k výměnám podskupin. Jako aktivní místa jsou definována prázdná pole, která mají na všech čtyřech sousedních polích právě jednu látku.
Do takového místa se může vázat látka, která je doplňková k podčástem, které do aktivního místa míří. K červené a modré podčásti jsou navzajém doplňkové, zelené a šedé podčásti jsou doplňkové samy sobě.
@obrazek_protein obsahuje vyznačené místo, do kterého by se vázala látka #box(image("compound_cssz.png")). Aktivní místa mohou být maximálně 4 (preferují se ty vlevo a poté nahoře).

Aktivní místa, která jsou maximálně 4 pole od sebe (opět přednostně zleva a shora), se zpárují a katalyzují reakci výměny podčástí mezi látkami. Vyměňují se ty podčásti, které jsou k sobě nejblíže.
Zbylá aktivní místa katalyzují reakci výměny vedlejších podčástí vázané látky, těch, které jsou nejblíže středu (těžišti) aktivních míst (přednostně nalevo nahoře).
Pro @obrazek_protein bude výsledek reakce #box(image("compound_cszs.png")).

Takto vytvořený model reakce umožňuje změnu podčástí podle struktury proteinu. Při mutaci může reakce, kterou protein provádí, zachovat, mírně pozměnit i radikálně změnit s nezanedbatelnými pravděpodobnostmi.
Je snadné ho implementovat a je nenáročný na výpočet. Také dokud nenastanou krajní připady, tak nezáleží na orientaci proteinu. (existuje ale významný krajní případ - jen jedno aktivní místo).

=== Allosterická místa

Proteinům by neměla chybět možnost chovat se různě podle látek v jejich prostředí (aby buňky mohly měnit chování podle podnětů). Proto je součástí modelu allosterická katalýza, která umožňuje
proteinům obsahovat receptor a měnit podle něj svou efektivitu. Allosterická místa jsou podobná aktivním - jsou to prázdná pole, která vedle sebe mají právě tři pole, která obsahují právě jednu
látku. To takového místa se mohou vázat 4 různé látky, které mají tři podčásti doplňkové k těm v proteinu. Katalytický efekt je určen jako $sin("vzdálenost allosterického od nejbližšího aktivního místa")$.
Sinus byl vybrán pro svůj průběh, nemění se moc rychle, ale mění se, aby se při posouvání allosterického místa postupně měnil katalytický efekt.

Podle katalytického efektu se určuje celkový efekt katalyzátorů na protein:

$ K_k = max(0, 1 + (sum^"kat."_i max(f_i (c_k), 0)) + (min^"kat."_i min(f_i (c_k), 0))) $

(součet f_i(c_k), ale započítaná je jen nejnižší hodnota). kde $c_k$ je koncentrace katalyzující látky v částici kde protein působí. $f_i$ je funkce toho, jak látka ovlivňuje průběh reakce proteinu podle své koncentrace.
podle katalytického efektu ($k_e$) se $f_i$ přelévá mezi

$ f_"aktivační" (c) = c^2 "(pro " k_e = 1 ")" $
$ f_"potřebný" (c) = c^2 - 1 "(pro " k_e = 0 ")" $
$ f_"inhibiční" (c) = -c "(pro " k_e = -1 ")" $

takto:

$ f_i (c) = "lerp"(f_"potřebný", f_"akivační", k_e) "pro" k_e > 0 $
$ f_i (c) = "lerp"(f_"potřebný", f_"inhibiční", "smoothstep"(-k_e)) "pro" k_e <= 0 $

Pro aktivátory je přechod mezi funkcemi lineární, pro inhibitory je přidaný smoothstep k interpolačnímu koeficientu, aby se urychlil přechod přes bod,
kde $f_i$ vychází přibližně jako záporná konstanta.

=== Reakce

Když protein působí v nějaké částici, katalyzuje v ní reakci. Předpokládá se, že bez pomoci proteinu běží reakce zanedbatelně.

O reakci jsou známé reaktanty a produkty, takže lze získat i změnu entalpie:

$ Delta H = sum^"produkty"_i E(i) - sum^"reaktanty"_i E(i) $

kde $E$ je již zmíněná energie látky. Když se zanedbá $Delta S dot T$ (předpokladem pro to je, že se entropie reakcí moc nezmění), tak z

$ Delta G = Delta H - Delta S dot T -> Delta G = Delta H $
$ K = e^(-(Delta G) / (R T)) -> K = e^(-(Delta H) / (R T)) $

jde spočítat rovnovážnou konstantu reakce. K té by se měl blížit poměr součinů koncentrací produktů a reaktantů, s rychlostí určenou sílou enzymatické katalýzy. Během kroku, kdy protein provádí danou reakci,
se spočítá aktuální poměr ($K''$) a během kroku se změní na

$ K' = "lerp"(K'', K, c_r dot K_k dot c_p) $

kde $c_r$ je konstanta rychlosti všech reakcí, $K_k$ je efekt katalyzátorů a $c_p$ je koncentrace proteinu. Nechť je $delta$ změna koncentrací v tomto kroku. Pak

$ K' = (product^"produkty"_i (c_(p i) + delta)) / (product^"reaktanty"_i (c_(r i) - delta)) $
$ K' dot (product^"reaktanty"_i (c_(r i) - delta)) - product^"produkty"_i (c_(p i) + delta) = 0 $

což je po roznásobení rovnice maximálně čtvrtého stupně v $delta$ (protože maximum jsou 4 aktivní místa) a tak je řešitelná. Z řešení se vybere takové $delta$, které je v absolutní hodnotě nejmenší a
zároveň nedostane žádnou koncentraci do záporné hodnoty. Kdyby mělo přesáhnout do záporu, tak se delta zmenší.

=== Genom <genom>

Jako návod pro sestavení proteinu slouží genom. Ten je tvořen z látek #box(image("genome_F.png")) a #box(image("genome_T.png")), které jsou přes zelené podčásti spojené do řady (v tomto modelu jsou jen 2 báze):

#figure(image("genome.png", width: 40%), caption: [ukázka úseku genetického kódu (FTTT/8)]) <geneticky_kod>

tento genetický kód začíná nalevo. Protože je aminokyselin více, než bází, tak se genetický kód dělí na kodony, které tu mají délku 4, aby mohly kódovat všech 14 zdejších aminokyselin. Genetický kód se přepisuje
pomocí F a T, kde F značí červenou bázi a T modrou. Pro přehlednější zápis je i zápis po kodonech. Číslo v hexadecimálním zápisu, tedy číslice nebo malé písmeno z rozsahu a-f značí jeden kodon. Každé číslo
převedeme na kodon tak, že do genetického kódu přidáme F v případě lichého a T v případě sudého čísla. Poté číslo vydělíme dvěma a tento postup ještě třikrát zopakujeme. Přiřazení aminokyselin ke kodonům
ukazuje #link(<extra_info>)[okno extra info] v aplkaci (v dokumentaci je i jeho obrázek).

Ke genomu se může vázat jiný typ proteinů, transkripční faktory. Pokud má protein na okraji obdélníku v mřížce, ve kterém se nachází, 7 po sobě jdoucích vystupujících
červených nebo modrých podčástí, stává se z něj transkripční faktor. Ten ovlivňuje projevy genomu v místě, kde se na genom váže. Tedy v místě, kde obsahuje komplementární
sekvenci bází (k červené se váže modrá a obráceně). Na ukázku genetického kódu (@geneticky_kod) by se vázal například tento protein, ten ale není dostatečně dlouhý na to,
aby opravdu byl transkripčím faktorem.

#figure(image("tf_part.png", width: 40%), caption: [aa5553])

Navázaný transkripční faktor blokuje v daném místě transkripci, z důvodu snadné implementace působí v místě nejblíže začátku genomu.

Pokud na okraji také obsahuje řadu alespoň tří zelených podčástí (ve stejném smyslu okraje jako v předchozích odstavcích), tak je transkripční faktor pozitivní - a od daného místa transkripci spouští.
Zelený kust okraje pak slouží jako místo na které nasedá ekvivalent RNA transkriptázy.

=== Speciální funkce

== Buňky

= Implementace

== Použité technologie

= Návod k použití

== Kompilace

Na sestavení projektu je potřeba mít nainstalované následující programy a knihovny (knihovny se hledají pomocí `find_package` v CMake):

 - kompilátor c++, který (dostatečně) podporuje c++23, například g++ 14 nebo novější nebo clang++ 18 nebo novější
 - CMake 3.22 nebo novější
 - sestavovací systém podporovaný cmake, například GNU make
 - knihovnu glm
 - knihovny pro vulkan (včetně vulkan-hpp verze 1.4.309 nebo novější), typicky z LunarG Vulkan SDK (#link("https://vulkan.lunarg.com/")), ale pro linux bývají v repozitářích
 - knihovnu vulkan memory allocator
 - knihovnu boost 1.81 nebo novější (stačí komponenta math)

Vše lze dohromady systému Ubuntu nainstalovat následujícím bash skriptem, ale pravděpodobně je v repozitářích několik z těchto věcí v moc starých verzích:

```bash
sudo apt install build-essential cmake
sudo apt install vulkan-tools libvulkan-dev spirv-tools
sudo apt install libglfw3-dev libglm-dev libboost-dev

git clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
cd VulkanMemoryAllocator
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --build build -t install
```

Tento projekt pak jde naklonovat a zkompilovat následujícím bash skriptem:

```bash
git clone --recurse-submodules https://github.com/nat-int/mikrosim.git
cd mikrosim
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DVULKAN_VALIDATION=OFF \
	-DCOMPILE_SHADERS=OFF
cmake --build build
```

Výsledný spustitenlný soubour pak je `./out/mikrosim`, případně `./out/mikrosim.exe`, (relativně k cestě po příkazu `cd mikrosim`). Je ho potřeba spouštět ze složky `./out/`.

== Návod k aplikaci

Po prvním spuštění vypadá aplikace přibližně takto:

#figure(image("app_on_open.png", width: 100%), caption: [aplikace po spuštění])

Obsahuje několik podoken, se kterými se dá levým tlačítkem myši pohybovat a připínat k hlavnímu oknu nebo do sebe navzájem. Mimo podoken je vidět zobrazení simulace (modrý čtverec za okny).

=== okno "Controls"

#figure(image("controls.png", width: 50%))

Okno "Controls" obsahuje seznam kláves/akcí pro interakci se zobrazením simulace s popisem jejich efektu.

=== okno "mikrosim"

#figure(image("mikrosim.png", width: 80%))

Okno mikrosim začíná počtem snímků v poslední vteřině, poté text říkající, zda simulace běží a tlačítko, které simulaci spustí, nebo zastaví.

Na druhém řádku je posuvník, na kterém se nastavuje počet kroků simulace, které se provedou mezi každými dvěma snímky (když simulace běží).

Na třetím řádku je posuvník, na kterém se nastavuje látka, která se právě zobrazuje. Pod ním je obrázek dané látky a energie uložená v jejích vazbách.

Další posuvník nastavuje koncentraci, na kterou klávesy G a B nastavují (když se stiskne klávesa B když žádné okno nemá "focus", tak se koncentrace všech proteinogeních látek nastaví na tuto hodnotu).

Následující 3 posuvníky nastavují vlastnosti tekutiny - hustotu, které se snaží dosáhnout, koeficient síly vyrovnávání hustoty a viskozitu.

Poslední posuvník nastavuje velikost částic při jejich vykreslování.

Pod posuvníky jsou tabulky s časy jednotlivých částí výpočtů simulace a vykreslování.

=== okno "effect blocks"

#figure(image("effect_blocks.png", width: 60%))

Toto okno obsahuje nastavení "efektových bloků", kterými se dá ovlivňovat simulace. Existují jich 2 typy: silové ("force blocks") a chemické ("chem blocks").
Silové bloky umožňují působit silou na částice v obdélníkové oblasti simulace. Chemické umožňují ovlivňovat koncentrace nějaké látky v obdélníkové oblasti simulace.
Od obou typů jsou dostupné 4 bloky. Bloky se vykreslují přes zobrazení simulace s vysokou průhledností, silové bloky červeně a chemické zeleně.

Na začátku jsou dvě textová pole s tlačítky - první umožňuje načíst nastavení všech bloků ze souboru, jehož cesta je napsána v textovém poli. Druhý umožňuje do souboru v textovém poli nastavení všech bloků uložit.

Pak následují nastavení silových bloků. Každé začíná pozicí a velikostí v osách x a y. Na třetím řádku posuvníků je nastavení homogení síly v osách x a y. Na čtvrtém řádku je nastavení síly kolem středu bloku (proti směru hodinových ručiček) a síla ke středu bloku. Síly jsou malé a tak chvilku trvá, než zapůsobí.

#figure(image("chem_block.png", width: 55%))

Poté následují nastavení chemických bloků. Stejně jako u silových bloků začínají dvěma řádky nastavení pozice a velikosti. Poté následuje cílová koncentrace, ke které se koncentrace přibližuje, poté síla, která určuje rychlost tohoto přibližování. Posuvník "direct add" udává koncentraci, která se přičítá ve všech částicím v oblasti. Nakonec je posuvník určující látku, kterou blok zrovna ovlivňuje (čísla látek jsou stejná jako v okně "mikrosim").

=== okno "extra info" <extra_info>

#figure(image("extra_info.png", width: 60%))

Okno extra info obsahuje tabulku pro převod mezi označením v genomu a látkou v proteinu.

=== okno "cell list"

#figure(image("cell_list.png", width: 40%))

Okno cell list obsahuje seznam všech živých buňek s jejich identifikačním číslem, pozicí a rychlostí.

=== okno "protein view"

#figure(image("protein_view.png", width: 50%))


=== okno "cell view"

=== zobrazení simulace

= Závěr

