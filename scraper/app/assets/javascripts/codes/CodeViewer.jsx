var CodeViewer = React.createClass({
	getLabelForCode: function(id, codes) {
		var label = "";
		
		for (var i = 0; i < codes.length; i++) {
			if (codes[i].id == id) {
				label = codes[i].label;
				break;
			}
		}
		
		return label;
	},
	
	render: function() {
		var getLabelForCode = this.getLabelForCode;
		var codes = this.props.codes;
		return (
				<div className="code-viewer">
				{CodeTypes.map(function(type) {
					if (type.id == 0) return null;
					var usedCodes = [];
					
					for (var accountId in codes) {
						if (typeof(codes[accountId][type.id]) !== "undefined") {
							for (var i = 0; i < codes[accountId][type.id].length; i++) {
								var code = getLabelForCode(codes[accountId][type.id][i], type.codes);
								if (usedCodes.indexOf(code) < 0) {
									usedCodes.push(code);
								}
							}
						}
					}
					
					if (usedCodes.length == 0) {
						usedCodes.push("[Not yet coded]");
					}
					
					return (
						<div>
							<div className="code-type">{type.label}</div>
							<ol className="code-list">
							    {usedCodes.map(function(code) {
							    	return (<li>{code}</li>);
							    })}
						    </ol>
					    </div>
				    );
				})}
			</div>
		);		
	}
});