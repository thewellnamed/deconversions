var Filters = React.createClass({
	_notCoded: 0,
	_opening: false,
	
	getInitialState: function() {
		var type = this.getCodeType(this.props.type);
		return { 
			type: type,
			optionsCount: this.getOptionsCount(type.codes), 
			filters: this.setInitialFilters(type.codes),
			modified: false
		};
	},
	
	setInitialFilters: function(codes) {
		var filters = this.props.filters.slice(0);
		return filters;
	},
	
	componentDidMount: function() {
		var toggle = this.refs.filterToggle;
		toggle.addEventListener('click', this.toggleDropDown);
		window.addEventListener('click', this.closeDropDown);
	},
	
	componentWillUnmount: function() {
		var toggle = this.refs.filterToggle;
		toggle.removeEventListener('click', this.toggleDropDown);
		window.removeEventListener('click', this.closeDropDown);
	},
	
	toggleDropDown: function(e) {
		var opening = !$(this.refs.filterDropDown).parent().hasClass('open');
		this._opening = opening;
		
		if (opening) {
			$(document.body).append('<div id="dropdown-mask"></div>');
		} else {
			this.handleCloseFilters();
		}
		
		
		$(this.refs.filterDropDown).parent().toggleClass('open');
		e.preventDefault();
	},
	
	closeDropDown: function(e) {			
		if ($(this.refs.filterDropDown).parent().hasClass('open') &&
		   !$(this.refs.filterDropDown).is(e.target) && 
			$(this.refs.filterDropDown).has(e.target).length === 0 &&
			!this._opening)
		{
			$(this.refs.filterDropDown).parent().removeClass('open');
			this.handleCloseFilters();
		}
		
		this._opening = false;
	},
	
	handleCloseFilters: function() {
		$('#dropdown-mask').remove();
		
		if (this.state.modified) {
			var filters = this.state.filters;
			
			if (filters.length == this.state.optionsCount) {
				filters = [];
			}
			
			this.props.onFilterChange(this.state.type.id, filters);
			this.setState({ modified: false});
		}
	},
	
	getCodeType: function(type) {
		var type = {};
		for (var i = 0; i < CodeTypes.length; i++) {
			if (CodeTypes[i].id == this.props.type) {
				type = CodeTypes[i];
				break;
			}
		}
		
		return type;
	},
	
	getOptionsCount: function(codes) {
		var count = 0;
		
		for (var i = 0; i < codes.length; i++) {
			var code = codes[i];
			if (typeof(code.filterUIPanel) !== "undefined" && code.id > 0) {
				count++;
			}
		}
		
		return count + 1;
	},
	
	handleFilterChange: function(code, e) {
		var filters = this.state.filters;
		if (e.target.checked) {
			filters.push(code);
		} else {
			filters.splice(filters.indexOf(code), 1);
		}
		
		this.setState({ filters: filters, modified: true });
	},
	
	toggleSelectAll: function(e) {
		var filters = this.state.filters;
		var codes = this.state.type.codes;
		
		if (filters.length < this.state.optionsCount) {
			for (var i = 0; i < codes.length; i++) {
				var code = codes[i];
				if (code.id > 0 && typeof(code.filterUIPanel) !== "undefined" && filters.indexOf(code.id) < 0) {
					filters.push(code.id);
				}
			}
			
			if (filters.indexOf(this._notCoded) < 0) {
				filters.push(this._notCoded);
			}
		} else {
			filters = [];
		}
		
		this.setState({ filters: filters, modified: true });
		e.preventDefault();
	},
	
	refresh: function(e) {
		$(this.refs.filterDropDown).parent().removeClass('open');
		$('#dropdown-mask').remove();
		this.props.onFilterChange(this.state.type.id, this.state.filters);
		e.preventDefault();
	},
	
	reset: function(e) {
		this.setState({ filters: [], modified: false });
		$(this.refs.filterDropDown).parent().removeClass('open');
		$('#dropdown-mask').remove();
		this.props.onFilterChange(this.state.type.id, []);
		e.preventDefault();
	},
	
	getFilterLabel: function(codeType, code) {
		var name = codeType.label + code.id;
		var label = typeof(code.shortLabel) !== "undefined" ? code.shortLabel : code.label;
		return (
			<label className="filter-option" for={name}>
				<input type="checkbox" name={name} checked={this.state.filters.indexOf(code.id) >= 0 } onChange={this.handleFilterChange.bind(null, code.id)} />
				<span>{label}</span>
			</label>
		);
	},
	
	render: function() {	
		var codeType = this.state.type;
		var codes = codeType.codes;
		var panels = [], i;
		for (i = 1; i <= this.state.type.filterUIPanelCount; i++) {
			panels[i] = [];
		}
		
		panels[1].push(this.getFilterLabel(codeType, { id: this._notCoded, shortLabel: "Not coded" }));
		
		for (i = 0; i < codes.length; i++) {
			var code = codes[i];
			if (typeof(code.filterUIPanel) !== "undefined") {
				if (code.id > 0) {
					panels[code.filterUIPanel].push(this.getFilterLabel(codeType, code));
				} else {
					panels[code.filterUIPanel].push(<div><span>&nbsp;</span></div>);
				}
			}
		}
		
		var select = "select all";
		if (this.state.filters.length == this.state.optionsCount) {
			select = "deselect all";
		}
		
		var colClass = "col-xs-" + (12/codeType.filterUIPanelCount);
		var dropdownClass = "dropdown filters " + codeType.cssClass;
		
		return (
			<li className={dropdownClass}>
				<a href="#" className="dropdown-toggle" ref="filterToggle" role="button">{codeType.filterLabel} <span className="caret"></span></a>
		        <div className="dropdown-menu" ref="filterDropDown">
	        		<div className="filter-select-option">
	        			<a href="#" onClick={this.toggleSelectAll}>{select}</a> | <a href="#" onClick={this.refresh}>refresh</a> | <a href="#" onClick={this.reset}>reset</a>
	        		</div>
		        	<div className="row">
		        		{panels.map(function(panel) {
		        			return (<div className={colClass}>{panel}</div>);
		        		})}
		        	</div>
		        </div>
			</li>
		);
	}
});