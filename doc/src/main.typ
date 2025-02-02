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
#set text(size: 12pt)
#set par(justify: true, first-line-indent: 0.2em, spacing: 1.5em)
#set heading(numbering: "1.")

#let title = [Simulating microorganisms]

#set document(title: title, author: "Filip Majer", keywords: ())
#show outline.entry.where(level: 1): it => { v(12pt, weak: true); strong(it); }

#align(center, [#text(20pt)[*#title*]\
	Filip Majer\
	Gymnázium Jana Keplera])

#align(center, [_this documentation is unfinished_])

#align(bottom, outline(indent: auto))
#pagebreak()

= Introduction

This document describes the mikrosim app avaliable at #link("https://github.com/nat-int/mikrosim"),
which is a simplified microorganism simulator.

== Motivation

Life and our world in general is complicated and for some purposes, it is a good idea to reduce the
complexity into a manageable state, for example studying evolution in specific environments, or with
truer models in medicine. Such models can then be simulated on computers which allows us to set
parameters exactly how we want them and look inside the model directly, which is easier than doing
real-life experiments.

Moreover, the complexity of behavior of such simulations is just fun to watch and it is interesting to
search for various properties of the system that emerge from rules of the model.

== Goal of this project

Here, the goal is to create a model of microorganisms like bacteria which is not _too far off_ from
reality and then simulate that model in a way that allows simulation of many _bacteria_ at the same time
so that they can interact and evolve, even on low-end hardware.

It shall be a desktop application which shows the state of simulation so that there can be something
nice to look at and there should be a way to influence it or set the initial parameters.

= Related work

There are many projects that aim to simulate living creatures. Many of them model creatures by
predefining various types of cells and the creatures are few cells assembled together, for
example Biogenesis (#link("https://biogenesis.sourceforge.net/")) simulates creatures this way.

Only other type of multi-organism life simulators I have found have creatures built around neural
networks and so they attempt to recreate more complex creatures, like The bibites
(#link("https://thebibites.itch.io/the-bibites")).

A quite neat project which combines both of those approaches is ALIEN
(#link("https://alien-project.org/")), but unfortunately it uses CUDA and therefore requires
a Nvidia GPU.

= Implementation

== General overview of the model

For the model used in mikrosim, I have decided to use a particle-based fluid simulation for the
environment, add compounds into it where each particle carries concentration of each compound.
Then for the organisms, there are special core particles which have their own genome which
gets translated into simplified proteins it contains. The core then contains concentrations
of each protein and can synthesise them from compounds or let them act on the compounds. Some
proteins perform special action like core division, but they have to get energy from another
reaction in order to do it.

== Model

= User manual

