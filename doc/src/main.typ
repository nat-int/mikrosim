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
#show " k ": " k" + sym.space.nobreak
#show " s ": " s" + sym.space.nobreak
#show " v ": " v" + sym.space.nobreak
#show " z ": " z" + sym.space.nobreak
#set par(justify: true, first-line-indent: 0.2em, spacing: 1.5em)
#set heading(numbering: "1.")

#let title = [Grafická aplikace pro simulaci mikroorganismů]

#set document(title: title, author: "Filip Majer", keywords: ())
#show outline.entry.where(level: 1): it => { v(12pt, weak: true); strong(it); }

#align(center, [#text(20pt)[*#title*]\
	Simulování života mikroorganismů v modelovém mikrosvětě\
	Filip Majer\
	Vedoucí práce: Emil Miler\
	Gymnázium Jana Keplera])

#align(center, [#link("https://github.com/nat-int/mikrosim")])

#align(bottom, outline(indent: auto))
#pagebreak()

= Úvod

Život je plný komplexních systémů, ale pro jeho zkoumání je někdy užitečné mít zjednodušený model, který jsme schopni plně propočítat. Naše schopnosti počítání posunuly vpřed počítače,
kterým dnes už můžeme zadávat k propočítání i poměrně komplikované modely.

== Cíle

Cílem tohoto projektu je vytvořit grafickou aplikaci, která zobrazuje a simuluje život modelových mikroorganismů. Model je zvolen tak, aby nebyl moc daleko od středoškolského modelu, jak mikroorganismy fungují.
Zároveň je dostatečně jednoduchý, aby pomocí něj šlo zároveň simulovat více variant organismů i na běžně výkonných počítačích.

== Související práce

Existuje mnoho modelů i jejich simulátorů zjednodušeného života. Velká část z nich funguje tak, že se nadefinuje několik typů buněk, kde má každá své specifické chování. Z těchto modelových buněk se
poskládá organismus. Ten se pak může replikovat do stejné nebo podobné buněčné konfigurace. Jedním takovým simulátorem je Biogenesis (#link("https://biogenesis.sourceforge.net/")).

Jiná velká skupina modelů simuluje organismy jako objekty, které podle definovaných pravidel interagují s okolím a podle vstupů provádějí různé akce. Vstupy se typicky zpracovávají s využitím neuronových sítí. Příkladem
pro tuto skupinu je The bibites (#link("https://thebibites.itch.io/the-bibites")).

Oba tyto přístupy tak modelují spíše mnohobuněčné organismy než jednobuněčné mikroorganismy.

O něco obecnější model má projekt ALIEN (#link("https://alien-project.org/")), který je založený na simulaci částic, které mohou tvořit vazby a předávat si signály. ALIEN se snaží
o jednoduchý model a z něj vyvstávající chování, a tak v něm život není tak blízko realitě, jako bychom chtěli. Kromě toho je napsaný v CUDA (#link("https://developer.nvidia.com/cuda-toolkit")), takže vyžaduje počítač s grafickou
kartou od společnosti Nvidia.

= Model

Model je ve dvou dimenzích, aby bylo snadnější jej implementovat a přehledně vykreslovat.

== Prostředí

Mikroorganismy žijí v definovaném prostředí. Jako základ prostředí této simulace byla zvolena tekutina, protože i skutečné mikroorganismy často žijí ve vodě.
Standardní způsoby simulování tekutin jsou částicové, eulerovské (kontinuální) a kombinované. V tomto prezentovaném modelu byl použit částicový model z tohoto simulátoru:
#link("https://github.com/SebLague/Fluid-Sim/tree/Episode-01"), především proto, že daný model je snadno implementovatelný, zároveň je pro částicové simulace snažší zabránit tvorbě hmoty z ničeho.
Do prezentovaného modelu byl tento částicový model převzat bez korekcí shlukování částic při rychlých ztrátách tlaku, protože chování tekutiny příliš neovlivní a jejich vynecháním se model zjednodušší.
Snadnost implementace tu je na úkor realismu, tekutina je zde svými vlastnostmi mezi plynem, kapalinou a gelem. Přesto je dostatečně blízko reálné tekutině.

Tento model ve zkratce funguje tak, že tekutinu rozdělí na částice, kde každá z nich představuje malý objem tekutiny. Částice se zrychlují takovým směrem, aby se hustota (a tím i tlak) blížil požadované hodnotě.
Mimo to se rychlosti předávají mezi blízkými částicemi pro efekt viskozity.

Kromě tekutiny jsou v typickém prostředí, kde jednobuněčné mikroorganismy žijí, různé látky, ze kterých získávají energii nebo stavební materiály. Aby se simulace vešla do paměti a zároveň lépe paralelizovala,
jsou zde látky modelovány zjednodušeně jako čtveřice podčástí, které jsou červené, zelené, modré a šedé. Látky jsou symetrické vzhledem k rotaci (když se podčásti protočí, tak je to stále stejná látka).
Lze je zobrazit jako čtyřcípou hvězdu s cípy obarvenými na jednu ze čtyř barev.

#figure(image("compound.png", width: 15%), caption: ["látka" složená ze zelené, šedé, modré a modré podčásti])

Chemické chování látek (tvorba vazeb a vazebná energie) je inspirováno chováním malé organické látky, která obsahuje jeden uhlík se čtyřmi skupinami. Červená podčást se vlastnostmi blíží karboxylové skupině,
zelená hydroxylové, modrá aminové skuině a šedá neutrálnímu vodíku. Každé látce je přiřazena energie (slučovací entalpie) (uložená v jejích vazbách), která je určena takto:

$ sum^("podčásti")_(i) E_z(i) + c(i) dot (f("vedle i na jedné straně") + f("vedle i na druhé straně") + phi("naproti i")) $

$E_z$ dává energii vazby mezi podčástí a středem látky. $f$ určuje sílu indukčního efektu mezi vedlejšími pozicemi, $phi$ mezi protějšími pozicemi. $c$ dává sílu vlivu indukčního efektu z ostatních podčástí na podčást.
Hodnoty energií látek jsou tak rozmanité a jen občas shodné (ale jsou shodné pro překlopené látky) a můžeme získávat zajímavé výsledky při počítání s jejich rozdíly.

Každá částice (každá kapka tekutiny) v sobě může mít libovolné množství každé látky. V průběhu simulace látky difundují. Při rozptylování dané látky se koncentrace látky z každé částice
rovnoměrně rozmístí do všech částic v okruhu se stejným poloměrem, jaký je dosah jejich silových interakcí.

== Proteiny

Nejdůležitějšími funkčními jednotkami v buňkách v tomto modelu jsou proteiny. Ty se skládají z aminokyselin, které se zřetězí a složí do tvaru, který jim umožní pracovat.
Skládání proteinů je na tuto simulaci moc složité, a proto je zde model opět hodně zjednodušený. V tomto modelu jsou aminokyseliny definovány
jako látky vybrané z těch, které obsahují červenou a modrou podčást:

#figure(image("blocks.png", width: 100%), caption: [látky vybrané jako proteinogení])

Ze seznamu těchto látek pak skládáme protein tak, že za červený konec předchozí látky přidáme další látku tak, aby navazovala modrým koncem (jako translace probíhá od N-terminu k C-terminu za tvorby peptidických vazeb).
Protože jsou látky pravoúhlé, skládají se jejich lineární řetězce do mřížky, která simuluje jejich prostorovou strukturu. Když existuje více možností, jak by se mohla další látka navázat,
vybere se ta, která v řetězci červených a modrých podčástí pokračuje co nejdéle rovně.

#figure(image("protein.png", width: 40%), caption: [ukázka proteinu (aa19c6a80100e (více v #link(<genom>)[sekci genom])), žluté pole značí aktivní místo]) <obrazek_protein>

Ze struktury je určena stabilita proteinu, což je podíl proteinů, která se nerozloží během jednoho kroku simulace:

$ min(1 - 15% dot e^(-s_c), 99%), s_c = n/80 + n_e/4 $

kde $n$ je počet polí v mřížce, kde je více látek najednou (protein překrývá sám se sebou - kříží) a $n_e$ je počet míst, kde je více látek najednou a zároveň je vedle prázdného pole.
Pokud se řetězec proteinu překříží, znamená to, že je zabalenější a je náročnější ho rozkládat, takže je stabilnější. Když je překryv vedle prázdného pole, musí řetězec postupovat stejnou cestou,
struktura se zpevňuje a je o to stabilnější.

=== Aktivní místa

V modelových proteinech jsou aktivní místa, kde dochází k výměnám podskupin. Jako aktivní místa jsou definována prázdná pole, která mají na všech čtyřech sousedních polích právě jednu látku.
Do takového místa se může vázat látka, která je doplňková k podčástem, které do aktivního místa směřují. K červené je doplňková modá a naopak. Zelené a šedé podčásti jsou doplňkové samy sobě.
@obrazek_protein obsahuje vyznačené místo, do kterého by se vázala látka #box(image("compound_cssz.png")). Aktivní místa mohou být maximálně 4 (preferují se ta vlevo a poté nahoře).

V tomto modelu se aktivní místa, která jsou maximálně 4 pole od sebe (opět přednostně zleva a shora), spárují a katalyzují reakci výměny podčástí mezi látkami. Vyměňují se ty podčásti, které jsou k sobě nejblíže.
Zbylá aktivní místa katalyzují reakci výměny vedlejších podčástí vázané látky, tedy těch, které jsou nejblíže středu (těžišti) aktivních míst (přednostně nalevo nahoře).
Pro @obrazek_protein bude výsledek reakce #box(image("compound_cszs.png")).

Takto vytvořený model reakce umožňuje změnu podčástí podle struktury proteinu. Při mutaci může být reakce, kterou protein provádí, zachována, mírně pozměněna i radikálně změněna s nezanedbatelnými pravděpodobnostmi.
Takovýto model je snadné implementovat a je nenáročný na výpočet. Dokud nenastanou krajní připady, nezáleží na orientaci proteinu. (existuje ale významný krajní případ - jen jedno aktivní místo, pak na preferencích záleží).

=== Allosterická místa

Proteinům by neměla chybět možnost chovat se různě podle látek v jejich prostředí (aby buňky mohly měnit chování podle podnětů). Proto je součástí modelu allosterická modulace, která umožňuje
proteinům obsahovat receptor a měnit podle něj svou efektivitu. Allosterická místa jsou v tomto modelu podobná aktivním - jsou to prázdná pole, která vedle sebe mají právě tři pole, která obsahují právě jednu
látku. Do takového místa se mohou vázat 4 různé látky, které mají tři podčásti doplňkové k těm v proteinu. Modulační efekt je určen funkcí $sin("vzdálenost allosterického od nejbližšího aktivního místa")$.
Sinus byl vybrán pro svůj průběh, nemění se moc rychle, ale mění se, a aby se při posouvání allosterického místa postupně měnil modulační efekt.

Podle modulačního efektu se určuje celkový efekt allosterických modulátorů na protein:

$ M = max(0, 1 + (sum^"modulátory"_i max(f_i (c_m), 0)) + (min^"modulátory"_i min(f_i (c_m), 0))) $

(součet f_i(c_m), ale započítaná je jen nejnižší hodnota). kde $c_m$ je koncentrace modulující látky v částici kde protein působí. $f_i$ je funkce toho, jak látka ovlivňuje průběh reakce proteinu podle své koncentrace.
podle modulačního efektu ($m_e$) se $f_i$ přelévá mezi

$ f_"aktivační" (c) = c^2 "(pro " m_e = 1 ")" $
$ f_"potřebný" (c) = c^2 - 1 "(pro " m_e = 0 ")" $
$ f_"inhibiční" (c) = -c "(pro " m_e = -1 ")" $

takto:

$ f_i (c) = "lerp"(f_"potřebný", f_"akivační", m_e) "pro" m_e > 0 $
$ f_i (c) = "lerp"(f_"potřebný", f_"inhibiční", "smoothstep"(-m_e)) "pro" m_e <= 0 $

Pro allosterické aktivátory je přechod mezi funkcemi lineární, pro allosterické inhibitory je přidaný smoothstep k interpolačnímu koeficientu, aby se urychlil přechod přes bod,
kde $f_i$ vychází přibližně jako záporná konstanta.

=== Reakce

V tomto modelu mohou být uvnitř částic proteiny, které katalyzují reakce. Předpokládá se, že bez pomoci proteinu běží reakce zanedbatelně.

U reakcí jsou známé reaktanty a produkty, takže lze získat i změnu entalpie:

$ Delta H = sum^"produkty"_i Delta H (i) - sum^"reaktanty"_i Delta H (i) $

kde $Delta H$ je již zmíněná slučovací entalpie látky. Pokud se zanedbá $Delta S dot T$ (předpokladem pro to je, že se entropie během reakcí příliš nezmění), lze z

$ Delta G = Delta H - Delta S dot T -> Delta G = Delta H $
$ K = e^(-(Delta G) / (R T)) -> K = e^(-(Delta H) / (R T)) $

spočítat rovnovážnou konstantu reakce. K té by se měl blížit poměr součinů koncentrací produktů a reaktantů, s rychlostí určenou účinností enzymatické katalýzy. Během kroku, kdy protein provádí danou reakci,
se spočítá aktuální poměr ($K''$) a během kroku se změní na

$ K' = "lerp"(K'', K, c_r dot M dot c_p) $

kde $c_r$ je konstanta rychlosti všech reakcí, $M$ je efekt modulátorů a $c_p$ je koncentrace proteinu. Nechť je $delta$ změna koncentrací v tomto kroku. Pak

$ K' = (product^"produkty"_i (c_(p i) + delta)) / (product^"reaktanty"_i (c_(r i) - delta)) $
$ K' dot (product^"reaktanty"_i (c_(r i) - delta)) - product^"produkty"_i (c_(p i) + delta) = 0 $

což je po roznásobení rovnice maximálně čtvrtého stupně v $delta$ (protože maximum jsou 4 aktivní místa) a tak je řešitelná. Z řešení se vybere takové $delta$, které je v absolutní hodnotě nejmenší a
zároveň nedostane žádnou koncentraci do záporné hodnoty. Pokud by měla přesáhnout do záporu, $delta$ se zmenší.

=== Genom <genom>

Jako návod pro sestavení proteinu slouží genom. Ten je tvořen z látek #box(image("genome_F.png")) a #box(image("genome_T.png")), které jsou přes zelené podčásti spojené do řady (v tomto modelu jsou jen 2 báze, červená a modrá):

#figure(image("genome.png", width: 40%), caption: [ukázka úseku genetického kódu (FTTT/8)]) <geneticky_kod>

tento genetický kód začíná nalevo. Protože je aminokyselin více, než bází, genetický kód se dělí na kodony, které tu mají délku 4, aby mohly kódovat všech 14 zdejších aminokyselin. Genetický kód se přepisuje
pomocí F a T, kde F značí červenou bázi a T modrou. Pro přehlednější zápis je i zápis po kodonech. Číslo v hexadecimálním zápisu, tedy číslice nebo malé písmeno z rozsahu a-f, značí jeden kodon. Každé číslo
převedeme na kodon tak, že do genetického kódu přidáme F v případě lichého a T v případě sudého čísla. Poté číslo vydělíme dvěma a tento postup ještě třikrát zopakujeme. Přiřazení aminokyselin ke kodonům
ukazuje #link(<extra_info>)[okno "extra info"] v aplkaci (v dokumentaci je i jeho obrázek).

Ke genomu se může vázat jiný typ proteinů, transkripční faktory. Pokud má protein na okraji obdélníku v mřížce, ve kterém se nacházejí, 7 po sobě jdoucích vystupujících
červených nebo modrých podčástí, stává se z něj transkripční faktor. Ten ovlivňuje projevy genomu v místě, kde se na genom váže. Tedy v místě, kde obsahuje komplementární
sekvenci bází (k červené se váže modrá a obráceně). Na ukázku genetického kódu (@geneticky_kod) by se vázal například tento protein, ten ale není dostatečně dlouhý na to,
aby opravdu byl transkripčím faktorem.

#figure(image("tf_part.png", width: 40%), caption: [aa5553])

Navázaný transkripční faktor blokuje v daném místě transkripci, z důvodu snadné implementace působí v místě nejblíže začátku genomu.

Pokud na okraji také obsahuje řadu alespoň tří zelených podčástí (ve stejném smyslu okraje jako v předchozích odstavcích), je transkripční faktor pozitivní a od daného místa transkripci spouští.
Zelená část okraje slouží jako místo, na které nasedá ekvivalent RNA transkriptázy.

== Buňky

Modelové buňky obsahují centrum, což je speciální typ částice. Každá buňka má svůj genom, který kóduje několik proteinů. Začátek místa transkripce v genomu značí 6 po sobě jdoucích červených bází
(počítá se od konce úseku) (nebo pozitivní transkripční faktor). Od tohoto místa se genom čte po kodonech. Když se narazí na kodon `a`, který značí start kodon, začnou se do proteinu zapojovat
další látky podle kodonů, až do kodonu `e` (stop kodonu) včetně.

Každá buňka může obsahovat libovolné množství každého proteinu zakódovaného ve svém genomu, podobně jako to je u látek. Během každého kroku se proteiny mohou tvořit a rozpadat.
Rozpadne se podíl proteinů podle již zmíněné stability. Vytvořit se může $1/2$ jednotek množství proteinů za krok, $M' dot c'_p$, pokud je tvorba daného proteinu podmíněna pozitivním traskripčním faktorem.
$M'$ je efekt allosterických modulátorů na transkripční faktor a $c'_p$ je jeho koncentrace. Toto množství se sníží o součet $M' dot c'_p$ všech negativních transkripčních faktorů na celém úseku, který protein kóduje.
Toto množství se sníží v případě, že buňka neobsahuje dostatečné množství látek na tvorbu proteinu.

Modelové buňky potřebují k napodobení života ještě několik funkcí, které nespadají pod katalyzátory (enzymy) a transkripční faktory. Jednou z nich je dělení, při kterém se musí nakopírovat celý genom.
Genom zde kopírují "genomové polymerázy", což jsou proteiny, které mají nanejvýš čtyři pole od sebe aktivní místa pro obě látky, ze kterých se tvoří genom (#box(image("genome_F.png")) a #box(image("genome_T.png")))
(takže "pracují s genomem").
Tato aktivní místa se nepodílejí na reakcích. Kromě toho musí provádět nějakou jinou reakci, ze které získají energii pro tvorbu nového genomu. Vždy, když tuto reakci provede, namnoží se kousek genomu (nebo
dojde k mutaci). Kopírování probíhá od začátku, postupně.
Za každý kousek, k nakopírování je malá šance, že se přeskočí báze nebo kodon, že se přidá náhodná báze nebo kodon a nebo, nebo že místo, odkud se právě kopíruje, skočí na náhodné místo v celém genomu.
Při přidávání bází určuje šance na vybrání báze jejich koncentrace (koncentrace látky je váha při náhodném výběru).
Při normálním vkládání další báze se také naváže náhodná báze, ale s velkou pravděpodobností se odváže, pokud je špatně. Když se odváže, nastane další pokus.

Buňky v modelu mají body zdraví. S každým krokem simulace 2 body ztratí, což představuje poškozující vliv prostředí - genom se může rozpadat nebo poškozovat a je potřeba ho udržovat.
Údržbu provádí opravovací proteiny, které mají alespoň 4 pole dlouhý úsek červených a modrých konců (jako mají transkripční faktory, ale delší) a aktivní místo na některou z látek,
ze kterých je tvořen genom (#box(image("genome_F.png")) nebo #box(image("genome_T.png"))). Toto aktivní místo se pak nepodílí v reakcích. Když tento protein provede
jinou reakci, ze které se uvolňí energie, dostane buňka, ve které protein je, určité množství bodů zdraví (podle uvolněné energie).

=== Struktury

Kromě centra může modelová buňka obsahovat jiné struktury. První z nich je "velká struktura". Tou je další částice, která je k centru vázána "pružinou", představující cytoskelet.
Velká struktura se sestavuje z "proteinů velké struktury", které mají tvar čtverce velikosti 5x5 polí.

#figure(image("struct_proteins.png", width: 40%), caption: [proteiny struktury, nalevo velké, napravo malé (růžová značí překryv látek v jednom poli)])

Tyto proteiny neprovádějí žádné reakce a nemohou být modulovány. Když se v buňce vyrobí dostatek proteinů velké struktury, částice se vytvoří, ale může být pouze jedna na buňku.
Proteiny mohou být označeny tím, že jejich kód končí `3e`. Označené proteiny působí v částici struktury místo centra buňky. Značka se po vytvoření proteinu oddělí, takže
se protein chová tak, jako kdyby značku neměl.

Na velkou strukturu může buňka působit motory. To jsou proteiny, které jsou velké alespoň 5x5 polí a jsou bodově symetrické podle jejich středu. Zároveň musí provádět reakci, ze které získají energii.
Podle energie získané z reakce se zapůsobí silou na velkou strukturu ve směru kolem centra buňky. Pokud je protein označený, tak po směru hodinových ručiček, jinak proti směru hodinových ručiček.

Druhý typ struktury jsou strukturní částice. Ty vyrábí slučovač částic, což je protein, který obsahuje dvě modulační místa vedle sebe, do kterých zapadá tato látka:

#figure(image("struct.png", width: 15%), caption: [kombinace látek tvořící strukturní částice])

Tato modulační místa pak nefungují jako modulační místa. Při provedení reakce uvolňující energii se sestaví malé množství těchto větších látek (podle energie a množství látek, které v nich jsou). Poté, co se jich
sestaví dostatečné množství, přidá se strukturní částice. Když je slučovač označený, přidá se strukturní částice do řetízku za částici velké struktury. Když ne, přidá se do membrány (o té bude více v
následujícím odstavci).

Třetí typ struktury je "malá struktura". Malé struktury jsou částice, které jsou také k centru vázány pružinou, ale také se vážou navzájem a rozestavují se kolem centra. Podobně jako velké struktury
tvoří proteiny malé strukury, které mají tvar čtverce o velikosti 3x3 pole. Malé struktury slouží jako
základ pro membránu. Když se do membrány přidá strukturní částice, zapojí se do náhodného místa v cyklu malých struktur. Strukturní částice se může do membrány přidat, pokud má buňka alespoň 3
částice malé struktury.

#figure(image("cell.png", width: 15%), caption: [buňka se strukturou (barvy jsou popsané v části #link(<view>)[Zobrazení simulace])])

Buňka může mít maximálně 4 částice velké a malé struktury dohromady a maximálně 8 strukturních částic.

Když má buňka alespoň 4 strukturní částice v uzavřené (neroztrhlé) membráně, ztrácí body zdraví poloviční rychlostí, protože ji membrána chrání.

= Implementace

== Použité technologie

Pro implementaci byl zvolen jazyk C++, který umožňuje velkou míru abstrakce společně s velkou kontrolou nad průběhem programu.
Na sestavování projeku byl vybrán CMake, který má dobrou podpru na více platformách i pro většinu rozšířených knihoven.

Pro vykreslování a část simulování byl vybrán Vulkan, což je API pro práci s grafickými jednotkami podporující značnou část platforem i čipů. Skrze něj je možné grafické čipy nebo karty
ovládat podrobně, takže je možné jejich výpočetní potenciál efektivně využívat. Vulkan je náročnější používat přímo. Pro ulehčení práce byla použita knihovna Vulkan Memory Allocator a
základ projektu tvořila knihovna na vykreslování s Vulkanem, kterou jsem si před několika lety napsal, obsahující sadu abstrakcí k Vulkanu.
Na implementaci shaderů byl použit jazyk GLSL.

Na multiplatformní vytvoření okna aplikace byla použita knihovna GLFW pro její jednoduchost.

Na výpočty byly využity knihovny glm, a komponenta math z knihovny boost. Glm pomáhá lépe zapisovat efektivní výpočty z lineární algebry. Z boost math byly využity funkce na řešení
kvadratických, kubických a kvartických rovnic, které vyvstávají při výpočtech reakcí.

Na zobrazování informací a interakci s uživatelem byla vybrána knihovna ImGUI (#link("https://github.com/ocornut/imgui")), kterou je především snadné používat.

== Struktura kódu

V podsložkách `format`, `input`, `log`, `rendering` a `scripts` a souborech `util.cpp` a `util.hpp` se nachází již zmíněná (mírně upravená) knihovna s abstrakcemi k Vulkanu.

V podsložce `shaders` jsou zdrojové kódy shaderů a většina projektu je přímo ve složce `src`.

Vstup do programu se nachází v `main.cpp`, kde se jen nastaví náhoda a spustí aplikace z `app.cpp`.
V těchto souborech je především vykreslování a zpracovávání vstupů uživatele.

V `cell.cpp` je chování buňky a proteinů během simulace (bez interakce s jinými částicemi), v `protein.cpp` je statická analýza proteinů. `compounds.cpp` obsahuje několik funkcí pro práci s látkami.

`parse.cpp` definuje funkce pro načítání hodnot ze souborů.

V `particles.cpp` je kód pro řízení interakcí mezi částicemi.

`views.cpp` definuje složitější podokna ("protein view" a "cell view").

== Implementační detaily

Aby byla simulace numericky stabilní, je změna v čase mezi kroky simulace konstantní.

Pozice a interakce částic se počítají z většiny na grafické jednotce, aby jich mohlo být co nejvíce (interakce se lépe paralelizují). Procesor simuluje akce buňek a proteinů, které obsahují netriviální výpočty.

Algoritmus pro hledání blízkých částic byl použit tento: #link("https://ramakarl.com/pdfs/2014_Hoetzlein_FastFixedRadius_Neighbors.pdf"). Pro počítání prefixových součtů byl využit algoritmus z Nvidia GPU Gems 3 kapitoly 39 (#link("https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda")) bez oprav "bank" konfliktů, protože jsou tyto problémy a opravy specifické pro Nvidia hardware. Místo CUDA jsou tyto techniky implementovány v compute shaderech.

Rozpouštění látek probíhá tak, že se do paměti grafické jednotky nakopírují koncentrace 4 látek (4 jsou protože jsou grafické jednotky obvykle dělané na počítání se čtveřicemi čísel) a během počítání interakcí
částic se provede i rozpouštění. Při následujícím kroku simulace se koncentrace látek po rozpuštění zkopírují zpět do paměti prcesoru a nahrají se koncentrace následujících 4 látek. Takto se rozpouštěné látky postupně střídají.

Aby se urychlila práce na procesoru v případech, kdy buňky obsahují mnoho různých proteinů, zhodnotí se každým krokem jen jeden protein a buď vytváření, nebo aktivita proteinu. Proteiny se postupně procházejí po krocích a koeficienty jsou upravené tak, aby se započítaly změny za kroky, kdy se počítají jiné proteiny.

Do koncentrací látek, které se právě rozpouští nebo kopírují zpět po rozpouštění, nelze právě zapisovat,
protože by se přepsaly novými hodnotami (výsledky rozpouštění). Když lze zapisovat do všech "aminokyselin" (přibližně v $1/3$ případů),
vyhodnocuje se vytváření proteinů, jinak se počítají jejich aktivity. Pokud by měl nějaký protein měnit koncentraci látek, které se zrovna rozpouštějí, jeho vyhodnocení se vynechá.

= Návod k použití

== Kompilace

Na sestavení projektu je potřeba mít nainstalované následující programy a knihovny (knihovny se hledají pomocí `find_package` v CMake):

 - kompilátor c++, který (dostatečně) podporuje c++23, například g++ 14 nebo novější nebo clang++ 18 nebo novější
 - CMake 3.22 nebo novější (#link("https://cmake.org/download/"))
 - sestavovací systém, který je podporovaný cmake, například GNU make
 - knihovny pro vulkan (včetně vulkan-hpp verze 1.3.301 nebo novější), typicky z LunarG Vulkan SDK (#link("https://vulkan.lunarg.com/")), ale pro linux bývají v repozitářích
 - knihovnu vulkan memory allocator (#link("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator"))
 - knihovnu glfw3 (#link("https://www.glfw.org/"))
 - knihovnu glm (#link("https://github.com/g-truc/glm"))
 - knihovnu boost 1.81 nebo novější (stačí komponenta math) (#link("https://www.boost.org/users/download/"))

Vše lze dohromady systému Ubuntu nainstalovat následujícím bash skriptem, ale pravděpodobně je v repozitářích několik z těchto věcí v moc starých verzích a tak je bude potřeba nainstalovat jinak:

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

=== okno "mikrosim" <mikrosim_window>

#figure(image("mikrosim.png", width: 80%))

Okno mikrosim začíná počtem snímků v poslední vteřině, poté text říkající, zda simulace běží a tlačítko, které simulaci spustí, nebo zastaví.

Na druhém řádku je posuvník, na kterém se nastavuje počet kroků simulace, které se provedou mezi každými dvěma snímky (když simulace běží).

Na třetím řádku je posuvník, na kterém se nastavuje látka, která se právě zobrazuje. Pod ním je obrázek dané látky a energie uložená v jejích vazbách.

Další posuvník nastavuje koncentraci, na kterou klávesy G a B nastavují (když se stiskne klávesa B když žádné okno nemá "focus", tak se koncentrace všech proteinogeních látek nastaví na tuto hodnotu).

Následující 3 posuvníky nastavují vlastnosti tekutiny - hustotu, které se snaží dosáhnout, koeficient síly vyrovnávání hustoty a viskozitu.

Poslední posuvník nastavuje velikost částic při jejich vykreslování.

Pod posuvníky jsou tabulky s časy jednotlivých částí výpočtů simulace a vykreslování.

=== okno "effect blocks" <effect_blocks>

#figure(image("effect_blocks.png", width: 60%))

Toto okno obsahuje nastavení "efektových bloků", kterými se dá ovlivňovat simulace. Existují jich 2 typy: silové ("force blocks") a chemické ("chem blocks").
Silové bloky umožňují působit silou na částice v obdélníkové oblasti simulace. Chemické umožňují ovlivňovat koncentrace nějaké látky v obdélníkové oblasti simulace.
Od obou typů jsou dostupné 4 bloky. Bloky se vykreslují přes zobrazení simulace s vysokou průhledností, silové bloky červeně a chemické zeleně.

Na začátku jsou dvě textová pole s tlačítky - první umožňuje načíst nastavení všech bloků ze souboru, jehož cesta je napsána v textovém poli. Druhý umožňuje do souboru v textovém poli nastavení všech bloků uložit.

Pak následují nastavení silových bloků. Každé začíná pozicí a velikostí v osách x a y. Na třetím řádku posuvníků je nastavení homogení síly v osách x a y. Na čtvrtém řádku je nastavení síly kolem středu bloku (proti směru hodinových ručiček) a síla ke středu bloku. Síly jsou malé, takže chvilku trvá, než zapůsobí.

#figure(image("chem_block.png", width: 55%))

Poté následují nastavení chemických bloků. Stejně jako u silových bloků začínají dvěma řádky nastavení pozice a velikosti. Poté následuje cílová koncentrace, ke které se koncentrace přibližuje, poté síla, která určuje rychlost tohoto přibližování. Posuvník "direct add" udává koncentraci, která se přičítá ve všech částicím v oblasti. Nakonec je posuvník určující látku, kterou blok zrovna ovlivňuje (čísla látek jsou stejná jako v okně "mikrosim").

=== okno "extra info" <extra_info>

#figure(image("extra_info.png", width: 60%))

Okno extra info obsahuje tabulku pro převod mezi označením v genomu a látkou v proteinu.

=== okno "cell list"

#figure(image("cell_list.png", width: 40%))

Okno cell list obsahuje seznam všech živých buňek s jejich identifikačním číslem, pozicí a rychlostí.

=== okno "protein view" <protein_view>

#figure(image("protein_view.png", width: 50%))

Na začátku okna je ovládání generátoru proteinů. Po kliknutí na tlačátko "randomize" se začnou generovat náhodné proteiny délky nastavené na posuvníku vedle, dokud se nenajde takový, který splňuje podmínky.
Podmínky se nastavují ve čtyřech textových polích na druhém řádku. Nápověda k nim se zobrazí, když na ně najedete myší.

Následuje obrázek proteinu, který je v tomto okně vybraný. Pod ním je reakce, kterou katalyzuje a seznam
modulátorů s jejich efektem.

Dále je informace o tom, na jaký úsek genetického kódu se váže.

Pod ní je tabulka speciálních vlastností a hodnot proteinu.

Na konci je textové pole, ve kterém je seznam kodonů právě vybraného proteinu se zaškrtávacím políčkem. Když je políčko zaškrtlé, tak pole nelze upravovat.
Vedle jsou tlačítka "set", které vybere protein do okna a "log", které vypíše zprávu obsahující obsah pole na `stdout`.

=== okno "cell view" <cell_view>

#figure(image("cell_view.png", width: 50%))

Stejně jako #link(<effect_blocks>)[okno "effect blocks"], začíná toto okno poli na načítání a ukládání ze souboru. Při načítání ze souboru se ze souboru přečtou znaky označující kodony a báze (0-9, a-f, T, F)
a do okna se vybere buňka s takovým genomem. Soubor může obsahovat komentáře, které začínají středníkem a končí odřádkováním.

Poté je v okně tabulka informací o buňce - stav, identifikátor, pozice, rychlost, pozice místa, ze kterého se právě kopíruje genom, zdraví a
stavy membrány (první dvojice čísel je množství malé struktury a využité množství malé struktury,
druhá dvojice je pro velkou strukturu a poslední číslo je postup v tvorbě membrány).

Dále jsou zaškrtávací pole "follow" a "lock graph". Pokud je "follow" zaškrtnuto, pohled ve zobrazení následuje vybranou buňku. Když je zaškrtnuto "lock graph", graf metabolismu (o kterém bude řeč níže) se nepohybuje.

Následuje genom buňky nakreslený jenom barvami bází. Když se myší najede na protein v seznamu proteinů (o kterém také bude řeč později), podtrhne se žlutě místo, kde je v genomu kódován.
Pokud je to transkripční faktor, vyznačí se fialově místa, kam se váže. Zároveň jsou vyznačeny bílou místa, kde se působí negativní transkripční faktory a zelenou místa, kde působí pozitivní transkripční faktory.

Dále se nachází graf koncentrací látek ve vybrané buňce. Když se myší najede na protein v seznamu proteinů, tak se červeně vyznačí jeho reaktanty, zeleně produkty a modře modulátory.
Barvy zvýraznění se mísí, takže žlutě zvýrazněný sloupek znamená, že je látka reaktant i produkt.

Okno pokračuje grafem metabolismu, kde šestiúhelníky značí proteiny a vykreslují se šipky k proteinům od jejich reaktantů a od proteinů k jejich produktům. Šipky působí jako pružiny a látky a proteiny se od sebe odpuzují.
Žluté šipky značí, že protein během dané reakce provádí speciální akci. Držením kolečka myši lze pohybovat s pohledem v grafu, držením levého kolečka a pohybem myši vertikálně lze zvětšovat a zmenšovat zorné pohled.
Levým tlačítkem lze s proteiny a látkami pohybovat.

Další je seznam proteinů. Při kliknutí na tlačítko "show" se daný protein vybere do #link(<protein_view>)[okna "protein view"]. Dále seznam obsahuje koncentraci proteinů a zkrácený zápis toho, co protein dělá.

Nakonec je v okně tlačítko test, které provede testy na funkci počítající chemické reakce a případné chyby vypíše na `stdout`.

=== Zobrazení simulace <view>

#figure(image("view.png", width: 70%))

Na pozadí je šachovnice, kde strana každého políčka má délku stejnou, jako je dosah interakcí částic. Částice tekutiny se vykreslují modře až světle modře podle své koncentrace látky, která je vybrána
v #link(<mikrosim_window>)[okně "mikrosim"]. Buňky a částice velké struktury se vykreslují fialově až světle modře. Částice malé struktury se vykreslují oranžově. Strukturní částice se vykreslují žlutě.

Ve zobrazení lze držením kolečka myši nebo klávesami W (nahoru), S (dolu), A (vlevo) a D (vpravo) pohybovat s pohledem. Točením kolečkem myši se pohled přibližuje / oddaluje. Při držení levé klávesy shift
jsou pohyby (až na tah se zmáčknutým kolečkem myši) pomalejší.

Středníkem se provede jeden krok simulace, mezerníkem se spustí nebo pozastaví.

Klávesou N se v místě křížku uprostřed obrazovky přidá nová buňka s genomem zkopírovaným od vybrané buňky z #link(<cell_view>)[okna "cell view"], pokud vybraná buňka není mrtvá.

Klávesou G se nastaví koncentrace látek, ze kterých se tvoří genom, ve všech částicích na hodnotu nastavenou v #link(<mikrosim_window>)[okně "mikrosim"].
Klávesa B má podobný efekt, jen nastavuje koncentrace všech modelových aminokyselin.

= Závěr

Aplikace dokže simulovat modelové mikroorganismy, které jsou schopné v dobrých podmínkách přežívat a množit se. Do jisté míry se mohou adaptovat na podmínky nové.

= Seznam odkazů

(ke dni 3.3.2025) 

 - #link("https://biogenesis.sourceforge.net/")
 - #link("https://thebibites.itch.io/the-bibites")
 - #link("https://alien-project.org/")
 - #link("https://developer.nvidia.com/cuda-toolkit")
 - #link("https://github.com/SebLague/Fluid-Sim/tree/Episode-01")
 - #link("https://github.com/ocornut/imgui")
 - #link("https://ramakarl.com/pdfs/2014_Hoetzlein_FastFixedRadius_Neighbors.pdf") (odkaz již nefunguje, ale prezentace je stále na #link("https://web.archive.org/web/20250113223404/https://ramakarl.com/pdfs/2014_Hoetzlein_FastFixedRadius_Neighbors.pdf"))
 - #link("https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda")
 - #link("https://cmake.org/download/")
 - #link("https://vulkan.lunarg.com/")
 - #link("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator")
 - #link("https://www.glfw.org/")
 - #link("https://github.com/g-truc/glm")
 - #link("https://www.boost.org/users/download/")

