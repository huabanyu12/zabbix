<?php
/*
** Zabbix
** Copyright (C) 2001-2022 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/


/**
 * Returns the names of supported event sources.
 *
 * If the $source parameter is passed, returns the name of the specific source, otherwise - returns an array of all
 * supported sources.
 *
 * @param int $source
 *
 * @return array|string
 */
function eventSource($source = null) {
	$sources = [
		EVENT_SOURCE_TRIGGERS => _('trigger'),
		EVENT_SOURCE_DISCOVERY => _('discovery'),
		EVENT_SOURCE_AUTO_REGISTRATION => _('auto registration'),
		EVENT_SOURCE_INTERNAL => _x('internal', 'event source')
	];

	if ($source === null) {
		return $sources;
	}
	elseif (isset($sources[$source])) {
		return $sources[$source];
	}
	else {
		return _('Unknown');
	}
}

/**
 * Returns the names of supported event objects.
 *
 * If the $source parameter is passed, returns the name of the specific object, otherwise - returns an array of all
 * supported objects.
 *
 * @param int $object
 *
 * @return array|string
 */
function eventObject($object = null) {
	$objects = [
		EVENT_OBJECT_TRIGGER => _('trigger'),
		EVENT_OBJECT_DHOST => _('discovered host'),
		EVENT_OBJECT_DSERVICE => _('discovered service'),
		EVENT_OBJECT_AUTOREGHOST => _('auto-registered host'),
		EVENT_OBJECT_ITEM => _('item'),
		EVENT_OBJECT_LLDRULE => _('low-level discovery rule')
	];

	if ($object === null) {
		return $objects;
	}
	elseif (isset($objects[$object])) {
		return $objects[$object];
	}
	else {
		return _('Unknown');
	}
}

/**
 * Returns all supported event source-object pairs.
 *
 * @return array
 */
function eventSourceObjects() {
	return [
		['source' => EVENT_SOURCE_TRIGGERS, 'object' => EVENT_OBJECT_TRIGGER],
		['source' => EVENT_SOURCE_DISCOVERY, 'object' => EVENT_OBJECT_DHOST],
		['source' => EVENT_SOURCE_DISCOVERY, 'object' => EVENT_OBJECT_DSERVICE],
		['source' => EVENT_SOURCE_AUTO_REGISTRATION, 'object' => EVENT_OBJECT_AUTOREGHOST],
		['source' => EVENT_SOURCE_INTERNAL, 'object' => EVENT_OBJECT_TRIGGER],
		['source' => EVENT_SOURCE_INTERNAL, 'object' => EVENT_OBJECT_ITEM],
		['source' => EVENT_SOURCE_INTERNAL, 'object' => EVENT_OBJECT_LLDRULE]
	];
}

function get_events_unacknowledged($db_element, $value_trigger = null, $value_event = null, $ack = false) {
	$elements = ['hosts' => [], 'hosts_groups' => [], 'triggers' => []];
	get_map_elements($db_element, $elements);

	if (empty($elements['hosts_groups']) && empty($elements['hosts']) && empty($elements['triggers'])) {
		return 0;
	}

	$config = select_config();
	$options = [
		'output' => ['triggerid'],
		'monitored' => 1,
		'skipDependent' => 1,
		'limit' => $config['search_limit'] + 1
	];
	if (!is_null($value_trigger)) {
		$options['filter'] = ['value' => $value_trigger];
	}
	if (!empty($elements['hosts_groups'])) {
		$options['groupids'] = array_unique($elements['hosts_groups']);
	}
	if (!empty($elements['hosts'])) {
		$options['hostids'] = array_unique($elements['hosts']);
	}
	if (!empty($elements['triggers'])) {
		$options['triggerids'] = array_unique($elements['triggers']);
	}
	$triggerids = API::Trigger()->get($options);

	return API::Event()->get([
		'source' => EVENT_SOURCE_TRIGGERS,
		'object' => EVENT_OBJECT_TRIGGER,
		'countOutput' => true,
		'objectids' => zbx_objectValues($triggerids, 'triggerid'),
		'filter' => [
			'value' => $value_event,
			'acknowledged' => $ack ? 1 : 0
		]
	]);
}

/**
 *
 * @param array  $event								An array of event data.
 * @param string $event['eventid']					Event ID.
 * @param string $event['correlationid']			OK Event correlation ID.
 * @param string $event['userid]					User ID who generated the OK event.
 * @param string $event['name']						Event name.
 * @param string $event['acknowledged']				State of acknowledgement.
 * @param string $backurl							A link back after acknowledgement has been clicked.
 *
 * @return CTableInfo
 */
function make_event_details($event, $backurl) {
	$event_update_url = (new CUrl('zabbix.php'))
		->setArgument('action', 'acknowledge.edit')
		->setArgument('eventids', [$event['eventid']])
		->setArgument('backurl', $backurl)
		->getUrl();

	$config = select_config();

	$table = (new CTableInfo())
		->addRow([
			_('Event'),
			(new CDiv($event['name']))->addClass(ZBX_STYLE_WORDWRAP)
		])
		->addRow([
			_('Severity'),
			getSeverityCell($event['severity'], $config)
		])
		->addRow([
			_('Time'),
			zbx_date2str(DATE_TIME_FORMAT_SECONDS, $event['clock'])
		])
		->addRow([
			_('Acknowledged'),
			(new CLink($event['acknowledged'] == EVENT_ACKNOWLEDGED ? _('Yes') : _('No'), $event_update_url))
				->addClass($event['acknowledged'] == EVENT_ACKNOWLEDGED ? ZBX_STYLE_GREEN : ZBX_STYLE_RED)
				->addClass(ZBX_STYLE_LINK_ALT)
		]);

	if ($event['r_eventid'] != 0) {
		if ($event['correlationid'] != 0) {
			$correlations = API::Correlation()->get([
				'output' => ['correlationid', 'name'],
				'correlationids' => [$event['correlationid']]
			]);

			if ($correlations) {
				if (CWebUser::getType() == USER_TYPE_SUPER_ADMIN) {
					$correlation_name = (new CLink($correlations[0]['name'],
						(new CUrl('correlation.php'))
							->setArgument('correlationid', $correlations[0]['correlationid'])
							->getUrl()
					))->addClass(ZBX_STYLE_LINK_ALT);
				}
				else {
					$correlation_name = $correlations[0]['name'];
				}
			}
			else {
				$correlation_name = _('Correlation rule');
			}

			$table->addRow([_('Resolved by'), $correlation_name]);
		}
		elseif ($event['userid'] != 0) {
			if ($event['userid'] == CWebUser::$data['userid']) {
				$table->addRow([_('Resolved by'), getUserFullname([
					'alias' => CWebUser::$data['alias'],
					'name' => CWebUser::$data['name'],
					'surname' => CWebUser::$data['surname']
				])]);
			}
			else {
				$user = API::User()->get([
					'output' => ['alias', 'name', 'surname'],
					'userids' => [$event['userid']]
				]);

				if ($user) {
					$table->addRow([_('Resolved by'), getUserFullname($user[0])]);
				}
				else {
					$table->addRow([_('Resolved by'), _('Inaccessible user')]);
				}
			}
		}
		else {
			$table->addRow([_('Resolved by'), _('Trigger')]);
		}
	}

	$tags = makeTags([$event]);

	$table->addRow([_('Tags'), $tags[$event['eventid']]]);

	return $table;
}

function make_small_eventlist($startEvent, $backurl) {
	$config = select_config();

	$table = (new CTableInfo())
		->setHeader([
			_('Time'),
			_('Recovery time'),
			_('Status'),
			_('Age'),
			_('Duration'),
			_('Ack'),
			_('Actions')
		]);

	$events = API::Event()->get([
		'output' => ['eventid', 'source', 'object', 'objectid', 'acknowledged', 'clock', 'ns', 'severity', 'r_eventid'],
		'select_acknowledges' => ['userid', 'clock', 'message', 'action', 'old_severity', 'new_severity'],
		'source' => EVENT_SOURCE_TRIGGERS,
		'object' => EVENT_OBJECT_TRIGGER,
		'value' => TRIGGER_VALUE_TRUE,
		'objectids' => $startEvent['objectid'],
		'eventid_till' => $startEvent['eventid'],
		'sortfield' => ['clock', 'eventid'],
		'sortorder' => ZBX_SORT_DOWN,
		'limit' => 20,
		'preservekeys' => true
	]);

	$r_eventids = [];

	foreach ($events as $event) {
		$r_eventids[$event['r_eventid']] = true;
	}
	unset($r_eventids[0]);

	$r_events = $r_eventids
		? API::Event()->get([
			'output' => ['clock'],
			'source' => EVENT_SOURCE_TRIGGERS,
			'object' => EVENT_OBJECT_TRIGGER,
			'eventids' => array_keys($r_eventids),
			'preservekeys' => true
		])
		: [];

	$triggerids = [];
	foreach ($events as &$event) {
		$triggerids[] = $event['objectid'];

		$event['r_clock'] = array_key_exists($event['r_eventid'], $r_events)
			? $r_events[$event['r_eventid']]['clock']
			: 0;
	}
	unset($event);

	// Get trigger severities.
	$triggers = $triggerids
		? API::Trigger()->get([
			'output' => ['priority'],
			'triggerids' => $triggerids,
			'preservekeys' => true
		])
		: [];

	$severity_config = [
		'severity_name_0' => $config['severity_name_0'],
		'severity_name_1' => $config['severity_name_1'],
		'severity_name_2' => $config['severity_name_2'],
		'severity_name_3' => $config['severity_name_3'],
		'severity_name_4' => $config['severity_name_4'],
		'severity_name_5' => $config['severity_name_5']
	];
	$actions = getEventsActionsIconsData($events, $triggers);
	$users = API::User()->get([
		'output' => ['alias', 'name', 'surname'],
		'userids' => array_keys($actions['userids']),
		'preservekeys' => true
	]);

	foreach ($events as $event) {
		$duration = ($event['r_eventid'] != 0)
			? zbx_date2age($event['clock'], $event['r_clock'])
			: zbx_date2age($event['clock']);

		if ($event['r_eventid'] == 0) {
			$in_closing = false;

			foreach ($event['acknowledges'] as $acknowledge) {
				if (($acknowledge['action'] & ZBX_PROBLEM_UPDATE_CLOSE) == ZBX_PROBLEM_UPDATE_CLOSE) {
					$in_closing = true;
					break;
				}
			}

			$value = $in_closing ? TRIGGER_VALUE_FALSE : TRIGGER_VALUE_TRUE;
			$value_str = $in_closing ? _('CLOSING') : _('PROBLEM');
			$value_clock = $in_closing ? time() : $event['clock'];
		}
		else {
			$value = TRIGGER_VALUE_FALSE;
			$value_str = _('RESOLVED');
			$value_clock = $event['r_clock'];
		}

		$cell_status = new CSpan($value_str);

		/*
		 * Add colors to span depending on configuration and trigger parameters. No blinking added to status,
		 * since the page is not on autorefresh.
		 */
		addTriggerValueStyle($cell_status, $value, $value_clock, $event['acknowledged'] == EVENT_ACKNOWLEDGED);

		// Create link to Problem update page.
		$problem_update_url = (new CUrl('zabbix.php'))
			->setArgument('action', 'acknowledge.edit')
			->setArgument('eventids', [$event['eventid']])
			->setArgument('backurl', $backurl)
			->getUrl();

		$table->addRow([
			(new CLink(zbx_date2str(DATE_TIME_FORMAT_SECONDS, $event['clock']),
				'tr_events.php?triggerid='.$event['objectid'].'&eventid='.$event['eventid']
			))->addClass('action'),
			($event['r_eventid'] == 0)
				? ''
				: (new CLink(zbx_date2str(DATE_TIME_FORMAT_SECONDS, $event['r_clock']),
						'tr_events.php?triggerid='.$event['objectid'].'&eventid='.$event['eventid']
				))->addClass('action'),
			$cell_status,
			zbx_date2age($event['clock']),
			$duration,
			(new CLink($event['acknowledged'] == EVENT_ACKNOWLEDGED ? _('Yes') : _('No'), $problem_update_url))
				->addClass($event['acknowledged'] == EVENT_ACKNOWLEDGED ? ZBX_STYLE_GREEN : ZBX_STYLE_RED)
				->addClass(ZBX_STYLE_LINK_ALT),
			makeEventActionsIcons($event['eventid'], $actions['data'], $users, $severity_config)
		]);
	}

	return $table;
}

/**
 * Place filter tags at the beginning of tags array.
 *
 * @param array  $event_tags
 * @param string $event_tags[]['tag']
 * @param string $event_tags[]['value']
 * @param array  $f_tags
 * @param int    $f_tags[<tag>][]['operator']
 * @param string $f_tags[<tag>][]['value']
 *
 * @return array
 */
function orderEventTags(array $event_tags, array $f_tags) {
	$first_tags = [];

	foreach ($event_tags as $i => $tag) {
		if (array_key_exists($tag['tag'], $f_tags)) {
			foreach ($f_tags[$tag['tag']] as $f_tag) {
				if (($f_tag['operator'] == TAG_OPERATOR_EQUAL && $tag['value'] === $f_tag['value'])
						|| ($f_tag['operator'] == TAG_OPERATOR_LIKE
							&& ($f_tag['value'] === '' || stripos($tag['value'], $f_tag['value']) !== false))) {
					$first_tags[] = $tag;
					unset($event_tags[$i]);
					break;
				}
			}
		}
	}

	return array_merge($first_tags, $event_tags);
}

/**
 * Place priority tags at the beginning of tags array.
 *
 * @param array  $event_tags             An array of event tags.
 * @param string $event_tags[]['tag']    Tag name.
 * @param string $event_tags[]['value']  Tag value.
 * @param array  $priorities             An array of priority tag names.
 *
 * @return array
 */
function orderEventTagsByPriority(array $event_tags, array $priorities) {
	$first_tags = [];

	foreach ($priorities as $priority) {
		foreach ($event_tags as $i => $tag) {
			if ($tag['tag'] === $priority) {
				$first_tags[] = $tag;
				unset($event_tags[$i]);
			}
		}
	}

	return array_merge($first_tags, $event_tags);
}

/**
 * Create element with tags.
 *
 * @param array  $list
 * @param string $list[]['eventid'|'triggerid']
 * @param array  $list[]['tags']
 * @param string $list[]['tags'][]['tag']
 * @param string $list[]['tags'][]['value']
 * @param bool   $html
 * @param string $key                            Name of tags source ID. Possible values:
 *                                                - 'eventid' - for events and problems (default);
 *                                                - 'triggerid' - for triggers.
 * @param int    $list_tag_count                 Maximum number of tags to display.
 * @param array  $filter_tags                    An array of tag filtering data.
 * @param string $filter_tags[]['tag']
 * @param int    $filter_tags[]['operator']
 * @param string $filter_tags[]['value']
 * @param int    $tag_name_format                Tag name format. Possible values:
 *                                                - PROBLEMS_TAG_NAME_FULL (default);
 *                                                - PROBLEMS_TAG_NAME_SHORTENED;
 *                                                - PROBLEMS_TAG_NAME_NONE.
 * @param string $tag_priority                   A list of comma-separated tag names.
 *
 * @return array
 */
function makeTags(array $list, $html = true, $key = 'eventid', $list_tag_count = ZBX_TAG_COUNT_DEFAULT,
		array $filter_tags = [], $tag_name_format = PROBLEMS_TAG_NAME_FULL, $tag_priority = '') {
	$tags = [];

	if ($html) {
		// Convert $filter_tags to a more usable format.
		$f_tags = [];

		foreach ($filter_tags as $tag) {
			$f_tags[$tag['tag']][] = [
				'operator' => $tag['operator'],
				'value' => $tag['value']
			];
		}
	}

	if ($tag_priority !== '') {
		$p_tags = explode(',', $tag_priority);
		$p_tags = array_map('trim', $p_tags);
	}

	foreach ($list as $element) {
		CArrayHelper::sort($element['tags'], ['tag', 'value']);

		$tags[$element[$key]] = [];

		if ($html) {
			// Show first n tags and "..." with hint box if there are more.

			$e_tags = $f_tags ? orderEventTags($element['tags'], $f_tags) : $element['tags'];

			if ($tag_priority !== '') {
				$e_tags = orderEventTagsByPriority($e_tags, $p_tags);
			}

			$tags_shown = 0;

			foreach ($e_tags as $tag) {
				$value = getTagString($tag, $tag_name_format);

				if ($value !== '') {
					$tags[$element[$key]][] = (new CSpan($value))
						->addClass(ZBX_STYLE_TAG)
						->setHint(getTagString($tag));
					$tags_shown++;
					if ($tags_shown >= $list_tag_count) {
						break;
					}
				}
			}

			if (count($element['tags']) > $tags_shown) {
				// Display all tags in hint box.

				$hint_content = [];

				foreach ($element['tags'] as $tag) {
					$value = getTagString($tag);
					$hint_content[$element[$key]][] = (new CSpan($value))
						->addClass(ZBX_STYLE_TAG)
						->setHint($value);
				}

				$tags[$element[$key]][] = (new CSpan(
					(new CButton(null))
						->addClass(ZBX_STYLE_ICON_WZRD_ACTION)
						->setHint(new CDiv($hint_content), '', true, 'max-width: 500px')
					))->addClass(ZBX_STYLE_REL_CONTAINER);
			}
		}
		else {
			// Show all and uncut for CSV.

			foreach ($element['tags'] as $tag) {
				$tags[$element[$key]][] = getTagString($tag);
			}
		}
	}

	return $tags;
}

/**
 * Returns tag name in selected format.
 *
 * @param array  $tag
 * @param string $tag['tag']
 * @param string $tag['value']
 * @param int    $tag_name_format  PROBLEMS_TAG_NAME_*
 *
 * @return string
 */
function getTagString(array $tag, $tag_name_format = PROBLEMS_TAG_NAME_FULL) {
	switch ($tag_name_format) {
		case PROBLEMS_TAG_NAME_NONE:
			return $tag['value'];

		case PROBLEMS_TAG_NAME_SHORTENED:
			return substr($tag['tag'], 0, 3).(($tag['value'] === '') ? '' : ': '.$tag['value']);

		default:
			return $tag['tag'].(($tag['value'] === '') ? '' : ': '.$tag['value']);
	}
}
