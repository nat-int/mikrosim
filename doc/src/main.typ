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

#let title = [Simulování mikroorganismů]

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

Cílem tohoto projektu je vybrat a naimplementovat do grafické aplikace model mikroorganismů, takový, aby nebyl moc daleko od středoškolského modelu toho, jak mikroorganismy fungují,
ale zároveň byl dostatečně jednoduchý, aby podle něj šlo simulovat mnoho organismů i na běžně rozšířených počítačích.

== Související práce

Existuje hodně modelů i jejich simulátorů zjednodušeného života. Velká část z nich je udělaná tak, že se připraví několik typů buněk, kde má každá své specifické chování, ze kterých se
podle poskládá organismus. Ten se pak může replikovat do stejné nebo podobné konfigurace. Jedním takovým simulátorem je Biogenesis (#link("https://biogenesis.sourceforge.net/")).

Jiná velká skupina modelů simuluje organismy jako objekty, které podle nějakých pravidel pozorují okolí a podle toho provádí různé akce, typicky s využitím neuronových sítí. Příkladem
pro tuto skupinu je The bibites (#link("https://thebibites.itch.io/the-bibites")).

Oba tyto přístupy tak modelují spíše větší organismy než mikroorganismy.

O něco univerzálnější model má projekt ALIEN (#link("https://alien-project.org/")), který je založený na simulaci částic, které mohou tvořit vazby a předávat si informace. ALIEN se snaží
o jednoduchý model a z něj vyvstávající chování, a tak je v něm život není tak blízko realitě, jako bychom zde chctěli. Kromě toho je napsaný v CUDA, takže vyžaduje počítač s grafickou
kartou od společnosti Nvidia.

= Model

Model je ve dvou dimenzích, aby bylo možné ho přehledně vykreslovat a snáze implementovat.

== Prostředí

Mikroorganismy žijí v nějakém prostředí. Jako základ prostředí této simulace byla zvolena tekutina, protože se mnohým mikroorganismům žije dobře ve vodě.
Standardní způsoby simulování tekutin jsou částicové, eulerovské a kombinované, ze kterých byl vybrán částicový model z tohoto simulátoru: #link("https://github.com/SebLague/Fluid-Sim/tree/Episode-01"),
bez korekcí shlukování částic při rychlých ztrátách tlak, a to především proto, že je tento model poměrně snadný k implementaci, zároveň je pro částicové simulace snažší dosáhnout toho,
aby se nevytvářela hmota z ničeho. Snadnost implementace tu je na úkor realismu, tekutina se chová jako něco mezi plynem, kapalinou a želé, ale stále to je dostatečně blízko reálné tekutině.

Kromě vody jsou v typickém prostředí, kde mikroorgaismy žijí různé látky ze kterých získávají energii nebo stavební materiály. Aby se simulace vešla do paměti a zároveň lépe paralelizovala,
jsou zde látky zjednodušené na seřazené čtveřici podčástí, které jsou červené, zelené, modré a šedé, ale shodné pod rotací, takže reprezentují čtyřcípou hvězdu s cípy obarvenými na jednu ze čtyř barev.

#figure(image("compound.png", width: 15%), caption: ["látka" složená ze zelené, šedé, modré a modré podčásti])

Chování látek je pak inspirováno tím, jak se chová malá organická látka, která obsahuje jeden uhlík se čtyřmi skupinami, kde červená je trochu jako karboxylová skupina, zelená jako halogen
(u zelené to zrovna není tak moc), modrá jako aminová skuina a šedá jako vodík. Ke každé látce je přiřazena energie (uložená v jejích vazbách), která je určena:

$ sum^("podčásti")_(i) E_z(b_i) + c(b_i) dot (f(b_("na jednu stranu od i")) + f(b_("na druhou stranu od i")) + phi(b_("naproti i"))) $

kde $b$ je barva na dané pozici, a $E_z$, $c$, $f$ a $phi$ jsou funkce $"[barva]" -> ZZ$. $E_z$ dává energii vazby mezi podčástí a středem látky. Zbytek tohoto výpočtu je udělaný podle
indukčního efektu. $f$ určuje jeho sílu mezi vedlejšími pozicemi, $phi$ mezi protějšími pozicemi a $c$ sílu vlivu tohoto efektu na podčást. Energie látek jsou tak rozmanité a jen občas
shodné (ale jsou shodně pro ekvivalent enantiomerů) a můžeme získávat zajímavé výsledky při počítání s jejich rozdíly.

Každá částice v sobě může mít libovolné množství každé látky. V průběhu simulace se látky rozpouští, tak, že při kroku, během kterého se daná látka má rozpustit, tak se z každé částice
látka rovnoměrně rozloží do všech částic v okruhu se stejným poloměrem, jako je dosah jejich silových interakcí.

== Proteiny

Funkce buněk provádí proteiny. Ty se skládají z aminokyselin, které se zřetězí a složí do tvaru, který jim umožní pracovat. Skládání proteinů je na tuto simulaci moc složité, a tak je zde model opět hodně
zjednodušený. Z látek bylo vybráno několik jako aminokyseliny tvořící proteiny, které všechny obsahují červenou a modrou podčást:

#figure(image("blocks.png", width: 100%), caption: [látky vybrané jako proteinogení])

Ze seznamu těchto látek pak skládáme protein tak, že za červený konec předchozí látky přidáme další látku tak, aby navazovala modrým koncem, tak, jako se při translaci protein skládá od N-terminu a tvoří se
peptidické vazby, akorát zde s pevně daným tvarem. Když má látka více červených nebo modrých konců, tak se zapojí dvojice podčástí naproti sobě.

#figure(image("protein.png", width: 30%), caption: [ukázka proteinu (genom je aa19c6a80100e (více v #link(<genom>)[sekci genom])), žlutá tečka značí aktivní místo]) <obrazek_protein>

Ze struktury je určena stabilita proteinu - část, která vydrží krok simulace - takto:

$ min(1 - 15% dot e^(-s_c), 99%), s_c = n/80 + n_e/4 $

kde $n$ je počet míst, kde se protein překrývá sám se sebou a $n_e$ je počet míst, kde se překrývá a zároveň má vedle daného místa volno. To dává způsoby k velkému zvětšování stability i malým jednodušším
krokům, které stabilitu posunou o kousek dále, a reflektuje to, že zabalenější proteiny bývají stabilnější.

Jako aktivní místa jsou označeny ty vrcholy mřížky, ve které mohou být středy látek, které jsou ohraničeny ze všech čtyř stran právě jednou látkou
proteinu (řetězec látek se tam nekříží). Do takového místa se může vázat látka, která má podčásti doplňkové k podčástem, které do něj směřují z ohraničujících látek. K červené podčásti je doplňková
modrá a obráceně, k zelené části zelená a k šedé části šedá, přibližně na základě interpretace podčástí jako chemických skupin. @obrazek_protein obsahuje vyznačené místo, do kterého by se vázala látka
[červená, šedá, šedá, zelená]. Aktivní místa mohou být maximálně 4, když jich je více, tak se preferují ty vlevo a poté nahoře.

Aktivní místa, která jsou maximálně 4 místa od sebe (ob 2 místa, opět přednostně zleva a shora), se zpárují a katalyzují reakci výměny podčástmi mezi látkami, těch dvou, které jsou nejblíže
(s případnou preferencí vertikální výměny právě když je levé místo výše). Zbylá místa katalyzují reakci výměny vedlejších podčástí vázané látky, které jsou nejblíže středu (těžišti) aktivních míst
(opět přednostně nalevo nahoře). Pro @obrazek_protein bude výsledek reakce [červená, šedá, zelená, šedá].

Tento způsob umožňuje převod pořadí látek na funkci proteinu, které souvisí se strukturou, a tak se při mutaci může funkce zachovat, mírně pozměnit i radikálně změnit s nezanedbatelnými pravděpodobnostmi a je
implementačně i výpočetně poměrně jednoduchý. Také dokud nenastanou krajní připady (existuje ale významný krajní případ - jen jedno aktivní místo), tak je tento způsob invariantní pod rotací.

=== Allosterická místa

Funkce proteinů samotná může být aktivována i inhibována látkami. V tomto modelu jsou allosterická místa velmi podobná aktivním, jen mají v jednom směru prázdné místo, nebo překryté látky.
Katalyzátory jsou pak 4 látky které se do daného místa váží stejným způsobem jako v aktivních místech, z podobných důvodů. Katalytický efekt je určen jako
$sin("vzdálenost allosterického od nejbližšího aktivního místa")$. Sinus zde slouží jako docela hladká (s docela malou derivací) náhodná funkce, hladká aby malá změna ve struktuře mohla
jen mírně změnit katalytický efekt.

Podle katalytického efektu se určuje celkový efekt katalyzátorů na protein:

$ K_k = max(0, 1 + (sum^"kat."_i f_i (c_k) "když" f_i (c_k) > 0) + (min^"kat."_i f_i (c_k) "když" f_i (c_k) <= 0)) $

kde $c_k$ je koncentrace katalyzující látky v částici kde protein působí a

$ f_i (x) = x^2 + k_e - 1 "pro" k_e > 0 $
$ f_i (x) = "lerp"(x^2 - 1, -x, "smoothstep"(-k_e)) "pro" k_e <= 0 $

kde $k_e$ je katalytický efekt katalyzátoru. $f_i$ je lineárně interpolovaná funkce vytvořená za krajních funkcí koncentrace, kde $k_e = 1$ znamená, že katalyzátor reakci podporuje, $k_e = 0$ znamená,
že katalyzátor je k reakci potřeba a $k_e = -1$ znamená, že katalyzátor reakci inhibuje. Pro záporná $k_e$ je přidaný smoothstep k interpolačnímu koeficientu, aby se urychlyl přechod přes bod,
kde $f_i$ vychází přibližně jako záporná konstanta.

=== Reakce

Když protein působí v nějaké částici, katalyzuje v ní reakci. Předpokládá se, že reakce běží jen zanedbatelně bez pomoci proteinu, a tak protein spouští reakci. O reakci jsou známé reaktanty a produkty, takže
lze získat i změnu entalpie:

$ Delta H = sum^"produkty"_i E(i) - sum^"reaktanty"_i E(i) $

kde $E$ je již zmíněná energie látky. Když se zanedbá $Delta S dot T$, tak z

$ Delta G = Delta H - Delta S dot T -> Delta G = Delta H $
$ K = e^(-(Delta G) / (R T)) -> K = e^(-(Delta H) / (R T)) $

jde spočítat rovnovážnou konstantu reakce. K té by se měl blížit poměr součinů koncentrací produktů a reaktantů, s rychlostí určenou sílou enzymatické katalýzy. Během kroku, kdy protein provádí danou reakci,
se spočítá aktuální poměr ($K''$) a během kroku se změní na

$ K' = "lerp"(K'', K, c_r dot K_k dot c_p) $

kde $c_r$ je volitelná konstanta určující rychlosti všech reakcí, $K_k$ je efekt katalyzátorů a $c_p$ je koncentrace proteinu. Nechť je $delta$ změna koncentrací v tomto kroku. Pak

$ K' = (product^"produkty"_i (c_(p i) + delta)) / (product^"reaktanty"_i (c_(r i) - delta)) $
$ K' dot (product^"reaktanty"_i (c_(r i) - delta)) - product^"produkty"_i (c_(p i) + delta) = 0 $

což je po roznásobení rovnice maximálně čtvrtého stupně v $delta$ (protože maximum jsou 4 aktivní místa) a tak je řešitelná. Z řešení se vybere takové $delta$, které je v absolutní hodnotě nejmenší a
zároveň nedostane žádnou koncentraci do záporné hodnoty zmenšuje ho dle potřeby.

=== Genom <genom>

Buňky potřebují schopnost se replikovat, k čemuž jim slouží jejich genom, který slouží jako seznam všeho, z čeho buňka je. Zde je genom opět zjednodušený, protože musí být z "látek" a měl by
existovat způsob jeho interakce s proteiny, a je tvořen z látek [červená, zelená, šedá, zelená] a [modrá, zelená, šedá, zelená] vázaných do řady přes zelenou - takže tu jsou jen dvě možné báze.
Kromě toho má genom implicitně určený začátek.

Ke genomu se může vázat jiný typ proteinů, transkripční faktory. Pokud má protein na okraji osově zarovnaného obdélníku obalujícího protein alespoň 7 po sobě jdoucích vystupujících
červených nebo modrých podčástí, tak se z něj stává transkripční faktor a ovlivňuje projevy genomu v místě, kde se na genom váže, což je na genomu v místě, kde obsahuje komplementární
sekvenci bází (k červené se váže modrá a obráceně). Jsou dva možné směry orientace genomu oproti transkripčnímu faktoru, ze kterého je vybrán ten, který je shodný s tím, že je genom
poskládaný shora dolů s šedými podčástmi napravo (orientaci proteinu určuje jeho chiralita).

Navázaný transkripční faktor blokuje v daném místě transkripci, z důvodu snadné implementace působí v místě nejblíže začátku genomu.
Pokud však na okraji obsahuje řadu alespoň tří zelených podčástí (ve stejném smyslu jako v části, která se váže na genom), tak je transkripční faktor pozitivní a od daného místa transkripci spouští - opět
je to snadná podmínka a zelený kus okraje může být místo na které nasedá ekvivalent RNA transkriptázy.

=== Speciální funkce

== Buňky

= Implementace

== Použité technologie

= Návod k použití

== Kompilace

= Závěr

