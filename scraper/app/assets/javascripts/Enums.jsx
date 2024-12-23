var CodeType = {
	"Meta": 3,
	"Image": 1
}

var MetaCodes = [
	{ id: 0,	label: "" },
	{ id: 1, 	label: "Denial of Inequality", filterUIPanel: 1 },
	{ id: 2, 	label: "Men as Victims", filterUIPanel: 1 },
	{ id: 3, 	label: "Anti-Feminism", filterUIPanel: 1 },
	{ id: 4, 	label: "Pro-Men/Men's Rights", filterUIPanel: 1 }
];

var Codes = [
	{ id: 0,	label: "" },
	
	// Denial of Inequality
	{ id: -1,	label: "---" },
	{ id: 4,	label: "Denial of Inequality (MOVE TO META)" },
	{ id: 30,	label: "Denial of Wage Gap", shortLabel: "Denial - Wage Gap", filterUIPanel: 2 },
	{ id: 31,	label: "Denial of Existence of Sexism against Women", shortLabel: "Denial - Sexism", filterUIPanel: 2 },
	{ id: 2,	label: "Denial of Rape", shortLabel: "Denial - Rape", filterUIPanel: 2 },
	{ id: 3,	label: "Denial of Domestic Violence", shortLabel: "Denial - Domestic Violence", filterUIPanel: 2 },
	{ id: 22,	label: "Denial of Privilege - Women are More Privileged than Men", shortLabel: "Denial - Women Privileged", filterUIPanel: 2 },
	{ id: 19,	label: "Denial of Gendered Crime - Women Make False Accusations", shortLabel: "Denial - False accusations", filterUIPanel: 2 },
	
	// Men as Victims
	{ id: -1,	label: "---" },
	{ id: 5,	label: "Men as Victims (MOVE TO META)" },
	{ id: 24,	label: "Men as Victims of Sexism", shortLabel: "MAV - Sexism", filterUIPanel: 1 },
	{ id: 25,	label: "Men as Victims of Rape", shortLabel: "MAV - Rape", filterUIPanel: 1 },
	{ id: 26,	label: "Men as Victims of Domestic Violence", shortLabel: "MAV - Domestic Violence", filterUIPanel: 1 },
	{ id: 34, 	label: "Men as Victims of Harassment/Assault", shortLabel: "MAV - Assault/Harrassment", filterUIPanel: 1 },
	{ id: 27,	label: "Men as Victims of Disease", shortLabel: "MAV - Disease", filterUIPanel: 1 },
	{ id: 28,	label: "Men as Victims of Suicide", shortLabel: "MAV - Suicide", filterUIPanel: 1 },
	{ id: 1,	label: "Men as Victims of Hegemonic Gender Roles", shortLabel: "MAV - Gender Roles", filterUIPanel: 1 },
	{ id: 29,	label: "Men as Victims in Legal System", shortLabel: "MAV - Legal System", filterUIPanel: 1 },
	
	// Anti-Feminism
	{ id: -1,	label: "---" },
	{ id: 18,	label: "Anti-Feminism - Assertion of Hypocrisy", shortLabel: "AntiFem - Hypocrisy", filterUIPanel: 3 },
	{ id: 32,	label: "Anti-Feminism - Accusation of Misandry", shortLabel: "AntiFem - Misandry",  filterUIPanel: 3 },
	{ id: 6,	label: "Anti-Feminism - Misrepresentation of Feminism", shortLabel: "AntiFem - Misrepresentation", filterUIPanel: 3 },
	{ id: 7,	label: "Anti-Feminism - Backlash against Feminism", shortLabel: "AntiFem - Backlash", filterUIPanel: 3 },
	{ id: 20,	label: "Anti-Feminism - Misappropriating Feminist Discourse", shortLabel: "AntiFem - Misappropriation", filterUIPanel: 3 },
	{ id: 8,	label: "Anti-Feminism - Fallacy of Dramatic Instance", shortLabel: "AntiFem - Dramatic Instance", filterUIPanel: 3 },
	{ id: 15,	label: "Anti-Feminism - Reassertion of Patriarchy", shortLabel: "AntiFem - Patriarchy", filterUIPanel: 3 },
	{ id: 14,	label: "Anti-Feminism - False Equivalence", shortLabel: "AntiFem - False Equivalence", filterUIPanel: 3 },
	{ id: 16,	label: "Anti-Feminism - Post-feminism (We are all equal)", shortLabel: "AntiFem - Post-feminism", filterUIPanel: 3 },
		  		  
	// Pro-Men
	{ id: -1,	label: "---", filterUIPanel: 1 },
	{ id: 17,	label: "Pro-Men - Male Socialization", shortLabel: "ProMen - Socialization", filterUIPanel: 1 },
	{ id: 23,	label: "Pro-Men - Recognition of Men's Problems", shortLabel: "ProMen - Recognition", filterUIPanel: 1 },
	{ id: 21,	label: "Pro-Men - Call To Action/Activism", shortLabel: "ProMen - Call to Action", filterUIPanel: 1 },
		  
	// Misc
	{ id: -1,	label: "---", filterUIPanel: 2 },
	{ id: 9,	label: "Misc - Tyranny of the Snuggle" },
	{ id: 10,	label: "Misc - Child Support", filterUIPanel: 2 },
	{ id: 11,	label: "Misc - Child Custody", filterUIPanel: 2 },
	{ id: 35,   label: "Misc - Abortion", filterUIPanel: 2 },
	{ id: 36,   label: "Misc - Circumcision", filterUIPanel: 2 },
	{ id: 12,	label: "Misc - Intersectionality - Class", shortLabel: "Misc - Class", filterUIPanel: 2 },
	{ id: 13,	label: "Misc - Intersectionality - Race", shortLabel: "Misc - Race",  filterUIPanel: 2 },
	{ id: 33,	label: "Misc - Misrepresentation of Facts or Stats", shortLabel: "Misc - Bad Facts/Stats", filterUIPanel: 2 }
];

var CodeTypes = [
 	{ id: 3, label: "Meta", codes: MetaCodes, filterLabel: "Meta-Codes", cssClass: "meta-filters", filterUIPanelCount: 1 },
 	{ id: 1, label: "Image", codes: Codes, filterLabel: "Codes", cssClass: "code-filters", filterUIPanelCount: 3 }
 ];

var AccountPermission = {
	View: 1,
	Edit: 2,
	Admin: 4,
	
	check: function(account, perms) {
		return ((account.perms & perms) === perms);
	}
};




