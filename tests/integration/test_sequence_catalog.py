"""Keep sequence tools inside the actual bridge's supported schema vocabulary."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
import scene_editor_mcp_catalog as catalog
from scene_editor_mcp_compat import validate
class SequenceCatalog(unittest.TestCase):
    def test_vocabulary(self):
        vocabulary={'type','description','properties','required','additionalProperties','items','minItems','maxItems','minLength','maxLength','minimum','maximum','exclusiveMinimum','exclusiveMaximum','enum','const'}
        def check(schema):
            self.assertFalse(set(schema)-vocabulary);self.assertNotIsInstance(schema.get('type'),list)
            for child in schema.get('properties',{}).values():check(child)
            if isinstance(schema.get('items'),dict):check(schema['items'])
        for tool in catalog.tool_list():
            if tool['name'].startswith('sequence_'):check(tool['inputSchema'])
    def test_read_and_partial_apply(self):
        tools={t['name']:t['inputSchema'] for t in catalog.tool_list()}
        validate(tools['sequence_read'],{})
        validate(tools['sequence_read_section'],dict(section='clocks'))
        validate(tools['sequence_apply'],dict(expected_revision=1,changes=dict(camera_rig={},sequence=None)))
        for args in [dict(expected_revision=True,changes={}),dict(expected_revision=1,changes=dict(unknown={})),dict(expected_revision=1,changes=[] )]:
            with self.assertRaises(ValueError):validate(tools['sequence_apply'],args)
        with self.assertRaises(ValueError):validate(tools['sequence_read_section'],dict(section='missing'))
if __name__=='__main__':unittest.main()
