var CodeFilters = React.createClass({
	render: function() {
		return (
			<ul className="nav navbar-nav">
				<Filters type={CodeType.Meta} filters={this.props.filters[CodeType.Meta]} onFilterChange={this.props.onFilterChange} />
				<Filters type={CodeType.Image} filters={this.props.filters[CodeType.Image]} onFilterChange={this.props.onFilterChange} />
			</ul>
		);
	}
});